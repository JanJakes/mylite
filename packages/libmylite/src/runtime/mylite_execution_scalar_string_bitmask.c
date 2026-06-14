#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_string_position.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_string_bitmask.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct string_bitmask_scalar_text_argument {
    struct mylite_string_bitmask_slice slice;
    struct session_scalar_cell cell;
    char *owned_text;
};

static int string_bitmask_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_export_set_string_bitmask_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_result,
    size_t *out_result_length,
    bool *out_is_null
);
static int evaluate_make_set_string_bitmask_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_result,
    size_t *out_result_length,
    bool *out_is_null
);
static int evaluate_string_bitmask_integer_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_message,
    const char *range_message,
    int64_t *out_value,
    bool *out_is_null
);
static int evaluate_string_bitmask_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct string_bitmask_scalar_text_argument *out_argument
);
static int string_bitmask_set_owned_result(
    struct mylite_db *database,
    int rc,
    char *value,
    size_t value_length,
    bool is_null,
    struct session_scalar_cell *out_cell
);
static void string_bitmask_scalar_text_argument_deinit(
    struct string_bitmask_scalar_text_argument *argument
);
static enum planned_string_bitmask_function_kind string_bitmask_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
static bool is_string_bitmask_function_kind(enum mylite_sql_ast_node_kind ast_kind);

int mylite_execution_scalar_string_bitmask_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return string_bitmask_function_value(database, expression, out_cell);
}

enum planned_string_bitmask_function_kind mylite_execution_string_bitmask_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return string_bitmask_function_kind(ast_kind);
}

bool mylite_execution_is_string_bitmask_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return is_string_bitmask_function_kind(ast_kind);
}

static int string_bitmask_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum planned_string_bitmask_function_kind function_kind = PLANNED_STRING_BITMASK_FUNCTION_NONE;
    char *result = NULL;
    size_t result_length = 0U;
    bool result_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_BITMASK_FUNCTION_NONE
                                       : string_bitmask_function_kind(expression->kind);
    switch (function_kind) {
    case PLANNED_STRING_BITMASK_FUNCTION_EXPORT_SET:
        rc = evaluate_export_set_string_bitmask_function(
            database,
            expression,
            &result,
            &result_length,
            &result_is_null
        );
        break;
    case PLANNED_STRING_BITMASK_FUNCTION_MAKE_SET:
        rc = evaluate_make_set_string_bitmask_function(
            database,
            expression,
            &result,
            &result_length,
            &result_is_null
        );
        break;
    case PLANNED_STRING_BITMASK_FUNCTION_NONE:
        mylite_execution_set_unsupported_error(
            database,
            "string bitmask functions support EXPORT_SET and MAKE_SET"
        );
        return MYLITE_ERROR;
    }

    rc = string_bitmask_set_owned_result(
        database,
        rc,
        result,
        result_length,
        result_is_null,
        out_cell
    );
    return rc;
}

static int evaluate_export_set_string_bitmask_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_result,
    size_t *out_result_length,
    bool *out_is_null
) {
    static const struct mylite_string_bitmask_slice default_separator = {
        .text = ",",
        .length = 1U,
        .is_null = false,
    };
    const struct mylite_sql_ast_node *arguments = NULL;
    struct string_bitmask_scalar_text_argument on = {0};
    struct string_bitmask_scalar_text_argument off = {0};
    struct string_bitmask_scalar_text_argument separator = {0};
    struct mylite_string_bitmask_slice separator_slice = default_separator;
    int64_t bits_value = 0;
    int64_t count_value = 0;
    bool bits_is_null = false;
    bool count_is_null = false;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    if (out_result == NULL || out_result_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    *out_result_length = 0U;
    *out_is_null = false;

    if (expression == NULL || mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_native_function_parameter_count_error(database, "EXPORT_SET");
        return MYLITE_ERROR;
    }
    arguments = mylite_execution_child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        mylite_execution_set_native_function_parameter_count_error(database, "EXPORT_SET");
        return MYLITE_ERROR;
    }
    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count < string_bitmask_export_set_min_argument_count ||
        argument_count > string_bitmask_export_set_max_argument_count) {
        mylite_execution_set_native_function_parameter_count_error(database, "EXPORT_SET");
        return MYLITE_ERROR;
    }

    rc = evaluate_string_bitmask_integer_argument(
        database,
        mylite_execution_child_at(arguments, 0U),
        "EXPORT_SET() bitmask supports only signed integer, boolean, and NULL literals",
        "EXPORT_SET() bitmask literals must fit the signed 64-bit range",
        &bits_value,
        &bits_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_string_bitmask_text_argument(
            database,
            mylite_execution_child_at(arguments, 1U),
            &on
        );
    }
    if (rc == MYLITE_OK) {
        rc = evaluate_string_bitmask_text_argument(
            database,
            mylite_execution_child_at(arguments, 2U),
            &off
        );
    }
    if (rc == MYLITE_OK && argument_count >= 4U) {
        rc = evaluate_string_bitmask_text_argument(
            database,
            mylite_execution_child_at(arguments, 3U),
            &separator
        );
        separator_slice = separator.slice;
    }
    if (rc == MYLITE_OK && argument_count == string_bitmask_export_set_max_argument_count) {
        rc = evaluate_string_bitmask_integer_argument(
            database,
            mylite_execution_child_at(arguments, 4U),
            "EXPORT_SET() number_of_bits supports only signed integer, boolean, and NULL literals",
            "EXPORT_SET() number_of_bits literals must fit the signed 64-bit range",
            &count_value,
            &count_is_null
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_string_export_set_value(
            (uint64_t)bits_value,
            bits_is_null,
            on.slice,
            off.slice,
            separator_slice,
            count_value,
            count_is_null,
            argument_count == string_bitmask_export_set_max_argument_count,
            out_result,
            out_result_length,
            out_is_null
        );
    }

    string_bitmask_scalar_text_argument_deinit(&separator);
    string_bitmask_scalar_text_argument_deinit(&off);
    string_bitmask_scalar_text_argument_deinit(&on);
    return rc;
}

static int evaluate_make_set_string_bitmask_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_result,
    size_t *out_result_length,
    bool *out_is_null
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    struct string_bitmask_scalar_text_argument *values = NULL;
    struct mylite_string_bitmask_slice *slices = NULL;
    int64_t bits_value = 0;
    bool bits_is_null = false;
    size_t argument_count = 0U;
    size_t value_count = 0U;
    int rc = MYLITE_OK;

    if (out_result == NULL || out_result_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    *out_result_length = 0U;
    *out_is_null = false;

    if (expression == NULL || mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_native_function_parameter_count_error(database, "MAKE_SET");
        return MYLITE_ERROR;
    }
    arguments = mylite_execution_child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        mylite_execution_set_native_function_parameter_count_error(database, "MAKE_SET");
        return MYLITE_ERROR;
    }
    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count < 2U) {
        mylite_execution_set_native_function_parameter_count_error(database, "MAKE_SET");
        return MYLITE_ERROR;
    }
    value_count = argument_count - 1U;
    if (value_count > SIZE_MAX / sizeof(*values) || value_count > SIZE_MAX / sizeof(*slices)) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    values = (struct string_bitmask_scalar_text_argument *)calloc(value_count, sizeof(*values));
    slices = (struct mylite_string_bitmask_slice *)calloc(value_count, sizeof(*slices));
    if (values == NULL || slices == NULL) {
        free(values);
        free(slices);
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = evaluate_string_bitmask_integer_argument(
        database,
        mylite_execution_child_at(arguments, 0U),
        "MAKE_SET() bitmask supports only signed integer, boolean, and NULL literals",
        "MAKE_SET() bitmask literals must fit the signed 64-bit range",
        &bits_value,
        &bits_is_null
    );
    for (size_t value_index = 0U; rc == MYLITE_OK && value_index < value_count; ++value_index) {
        rc = evaluate_string_bitmask_text_argument(
            database,
            mylite_execution_child_at(arguments, value_index + 1U),
            &values[value_index]
        );
        if (rc == MYLITE_OK) {
            slices[value_index] = values[value_index].slice;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_string_make_set_value(
            (uint64_t)bits_value,
            bits_is_null,
            slices,
            value_count,
            out_result,
            out_result_length,
            out_is_null
        );
    }

    for (size_t value_index = 0U; value_index < value_count; ++value_index) {
        string_bitmask_scalar_text_argument_deinit(&values[value_index]);
    }
    free(values);
    free(slices);
    return rc;
}

static int evaluate_string_bitmask_integer_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_message,
    const char *range_message,
    int64_t *out_value,
    bool *out_is_null
) {
    int64_t value = 0;
    int rc = MYLITE_OK;

    if (out_value == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0;
    *out_is_null = false;
    if (!mylite_execution_string_slice_length_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    rc = mylite_execution_string_slice_signed_integer_value(
        database,
        expression,
        unsupported_message,
        range_message,
        &value,
        out_is_null
    );
    if (rc == MYLITE_OK) {
        *out_value = value;
    }
    return rc;
}

static int evaluate_string_bitmask_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct string_bitmask_scalar_text_argument *out_argument
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    int rc = MYLITE_OK;

    if (out_argument == NULL) {
        return MYLITE_MISUSE;
    }
    *out_argument = (struct string_bitmask_scalar_text_argument){0};

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (!mylite_execution_string_slice_scalar_text_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "string bitmask functions support only string, integer, boolean, NULL, session "
            "scalar, and system variable string arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "string bitmask functions support only string literals",
                "string bitmask function literals do not support NUL bytes",
                &out_argument->owned_text,
                &out_argument->slice.length
            );
            if (rc == MYLITE_OK) {
                out_argument->slice.text = out_argument->owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, &out_argument->cell);
    } else {
        rc = mylite_execution_string_length_session_scalar_argument_value(
            database,
            expression,
            &out_argument->cell
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_argument->cell.value == NULL) {
        out_argument->slice.is_null = true;
        return MYLITE_OK;
    }
    out_argument->slice.text = out_argument->cell.value;
    out_argument->slice.length = strlen(out_argument->cell.value);
    return MYLITE_OK;
}

static int string_bitmask_set_owned_result(
    struct mylite_db *database,
    int rc,
    char *value,
    size_t value_length,
    bool is_null,
    struct session_scalar_cell *out_cell
) {
    if (out_cell == NULL) {
        free(value);
        return MYLITE_MISUSE;
    }
    if (rc == MYLITE_NOMEM) {
        free(value);
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (rc != MYLITE_OK) {
        free(value);
        if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
            mylite_execution_set_runtime_error(
                database,
                "failed to evaluate string bitmask function"
            );
        }
        return rc;
    }
    if (is_null) {
        free(value);
        return MYLITE_OK;
    }
    if (value == NULL) {
        return MYLITE_MISUSE;
    }
    if (strlen(value) != value_length) {
        free(value);
        mylite_execution_set_runtime_error(
            database,
            "invalid NUL byte in string bitmask function result"
        );
        return MYLITE_ERROR;
    }
    out_cell->owned_text = value;
    out_cell->value = value;
    out_cell->value_size = value_length;
    out_cell->has_value_size = true;
    return MYLITE_OK;
}

static void string_bitmask_scalar_text_argument_deinit(
    struct string_bitmask_scalar_text_argument *argument
) {
    if (argument == NULL) {
        return;
    }
    free(argument->owned_text);
    mylite_execution_session_scalar_cell_deinit(&argument->cell);
    *argument = (struct string_bitmask_scalar_text_argument){0};
}

static enum planned_string_bitmask_function_kind string_bitmask_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_EXPORT_SET_FUNCTION:
        return PLANNED_STRING_BITMASK_FUNCTION_EXPORT_SET;
    case MYLITE_SQL_AST_MAKE_SET_FUNCTION:
        return PLANNED_STRING_BITMASK_FUNCTION_MAKE_SET;
    default:
        return PLANNED_STRING_BITMASK_FUNCTION_NONE;
    }
}

static bool is_string_bitmask_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return string_bitmask_function_kind(ast_kind) != PLANNED_STRING_BITMASK_FUNCTION_NONE;
}
