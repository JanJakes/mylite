#include "mylite_execution_scalar_string_transform.h"
#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_string_position.h"

#include "mylite_string_concat.h"
#include "mylite_string_insert.h"
#include "mylite_string_quote.h"
#include "mylite_string_replace.h"
#include "mylite_string_reverse.h"
#include "mylite_string_soundex.h"
#include "mylite_string_substring_index.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Static helper prototypes. */
static int concat_ws_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_concat_ws_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static bool concat_ws_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression);
static int string_replace_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int string_insert_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_string_replace_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static bool string_replace_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
);
static int evaluate_string_insert_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int evaluate_string_insert_position_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_position,
    bool *out_is_null
);
static int evaluate_string_insert_length_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_length,
    bool *out_is_null
);
static int string_reverse_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_string_reverse_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static bool string_reverse_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
);
static int soundex_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_soundex_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static bool soundex_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression);
static int quote_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_quote_scalar_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static bool quote_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression);
static int substring_index_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_substring_index_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int evaluate_substring_index_count_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_count,
    bool *out_is_null
);

int mylite_execution_scalar_concat_ws_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return concat_ws_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_string_replace_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return string_replace_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_string_insert_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return string_insert_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_string_reverse_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return string_reverse_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_soundex_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return soundex_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_quote_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return quote_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_substring_index_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return substring_index_function_value(database, expression, out_cell);
}

bool mylite_execution_concat_ws_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return concat_ws_scalar_argument_is_admitted(expression);
}

bool mylite_execution_string_replace_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return string_replace_scalar_argument_is_admitted(expression);
}

bool mylite_execution_string_reverse_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return string_reverse_scalar_argument_is_admitted(expression);
}

bool mylite_execution_soundex_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return soundex_scalar_argument_is_admitted(expression);
}

bool mylite_execution_quote_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return quote_scalar_argument_is_admitted(expression);
}

static int concat_ws_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *argument = NULL;
    struct mylite_string_concat_argument *concat_arguments = NULL;
    struct session_scalar_cell *cells = NULL;
    char **owned_texts = NULL;
    char *result = NULL;
    size_t argument_count = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONCAT_WS_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_native_function_parameter_count_error(database, "CONCAT_WS");
        return MYLITE_ERROR;
    }

    arguments = mylite_execution_child_at(expression, 0U);
    if (arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        mylite_execution_set_native_function_parameter_count_error(database, "CONCAT_WS");
        return MYLITE_ERROR;
    }
    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count < 2U) {
        mylite_execution_set_native_function_parameter_count_error(database, "CONCAT_WS");
        return MYLITE_ERROR;
    }
    if (argument_count > SIZE_MAX / sizeof(*concat_arguments) ||
        argument_count > SIZE_MAX / sizeof(*cells) ||
        argument_count > SIZE_MAX / sizeof(*owned_texts)) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    concat_arguments =
        (struct mylite_string_concat_argument *)calloc(argument_count, sizeof(*concat_arguments));
    cells = (struct session_scalar_cell *)calloc(argument_count, sizeof(*cells));
    owned_texts = (char **)calloc(argument_count, sizeof(*owned_texts));
    if (concat_arguments == NULL || cells == NULL || owned_texts == NULL) {
        free(concat_arguments);
        free(cells);
        free((void *)owned_texts);
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    argument = mylite_execution_child_at(arguments, 0U);
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < argument_count && argument != NULL;
         ++argument_index) {
        rc = evaluate_concat_ws_scalar_argument(
            database,
            argument,
            &cells[argument_index],
            &owned_texts[argument_index],
            &concat_arguments[argument_index].text,
            &concat_arguments[argument_index].text_length,
            &concat_arguments[argument_index].is_null
        );
        argument = argument->next_sibling;
    }
    if (rc == MYLITE_OK && argument != NULL) {
        mylite_execution_set_parse_error(database);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_string_concat_ws_value(database, concat_arguments, argument_count, &result);
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
    }
    if (rc == MYLITE_OK && result != NULL) {
        out_cell->owned_text = result;
        out_cell->value = out_cell->owned_text;
        result = NULL;
    }

    free(result);
    for (size_t argument_index = 0U; argument_index < argument_count; ++argument_index) {
        free(owned_texts[argument_index]);
        mylite_execution_session_scalar_cell_deinit(&cells[argument_index]);
    }
    free((void *)owned_texts);
    free(cells);
    free(concat_arguments);
    return rc;
}

static int evaluate_concat_ws_scalar_argument(
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
    if (!concat_ws_scalar_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "CONCAT_WS() supports only string, integer, boolean, NULL, session scalar, "
            "and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "CONCAT_WS() supports only string literals",
                "CONCAT_WS() literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = mylite_execution_string_length_session_scalar_argument_value(
            database,
            expression,
            inout_cell
        );
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

static bool concat_ws_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression) {
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_CONCAT_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_CONCAT_WS_FUNCTION) {
        return false;
    }
    return mylite_execution_string_length_scalar_argument_is_admitted(expression);
}

static int string_replace_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell cells[3] = {{0}, {0}, {0}};
    char *owned_texts[3] = {NULL, NULL, NULL};
    const char *texts[3] = {NULL, NULL, NULL};
    size_t text_lengths[3] = {0U, 0U, 0U};
    bool is_nulls[3] = {false, false, false};
    size_t result_length = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_REPLACE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 3U) {
        mylite_execution_set_native_function_parameter_count_error(database, "REPLACE");
        return MYLITE_ERROR;
    }

    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 3U; ++argument_index) {
        rc = evaluate_string_replace_scalar_argument(
            database,
            mylite_execution_child_at(expression, argument_index),
            &cells[argument_index],
            &owned_texts[argument_index],
            &texts[argument_index],
            &text_lengths[argument_index],
            &is_nulls[argument_index]
        );
    }
    if (rc == MYLITE_OK && !is_nulls[0] && !is_nulls[1] && !is_nulls[2]) {
        rc = mylite_string_replace_value(
            database,
            texts[0],
            text_lengths[0],
            texts[1],
            text_lengths[1],
            texts[2],
            text_lengths[2],
            &out_cell->owned_text,
            &result_length
        );
        (void)result_length;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
    } else if (rc == MYLITE_OK && out_cell->owned_text != NULL) {
        out_cell->value = out_cell->owned_text;
    }

    for (size_t argument_index = 0U; argument_index < 3U; ++argument_index) {
        free(owned_texts[argument_index]);
        mylite_execution_session_scalar_cell_deinit(&cells[argument_index]);
    }
    return rc;
}

static int string_insert_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell value_cell = {0};
    struct session_scalar_cell replacement_cell = {0};
    char *owned_value = NULL;
    char *owned_replacement = NULL;
    const char *value = NULL;
    const char *replacement = NULL;
    size_t value_length = 0U;
    size_t replacement_length = 0U;
    size_t result_length = 0U;
    int64_t position = 0;
    int64_t length = 0;
    bool value_is_null = false;
    bool position_is_null = false;
    bool length_is_null = false;
    bool replacement_is_null = false;
    bool helper_called = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_INSERT_STRING_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 4U) {
        mylite_execution_set_native_function_parameter_count_error(database, "INSERT");
        return MYLITE_ERROR;
    }

    rc = evaluate_string_insert_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &value_cell,
        &owned_value,
        &value,
        &value_length,
        &value_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_string_insert_position_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &position,
            &position_is_null
        );
    }
    if (rc == MYLITE_OK) {
        rc = evaluate_string_insert_length_argument(
            database,
            mylite_execution_child_at(expression, 2U),
            &length,
            &length_is_null
        );
    }
    if (rc == MYLITE_OK) {
        rc = evaluate_string_insert_scalar_argument(
            database,
            mylite_execution_child_at(expression, 3U),
            &replacement_cell,
            &owned_replacement,
            &replacement,
            &replacement_length,
            &replacement_is_null
        );
    }
    if (rc == MYLITE_OK && !value_is_null && !position_is_null && !length_is_null &&
        !replacement_is_null) {
        helper_called = true;
        rc = mylite_string_insert_value(
            database,
            &(struct mylite_string_insert_arguments){
                .value = {.text = value, .length = value_length},
                .position = position,
                .length = length,
                .replacement = {.text = replacement, .length = replacement_length},
            },
            &out_cell->owned_text,
            &result_length
        );
        (void)result_length;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
    } else if (helper_called && rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(database, "invalid UTF-8 value in INSERT()");
    } else if (out_cell->owned_text != NULL) {
        out_cell->value = out_cell->owned_text;
    }

    free(owned_value);
    free(owned_replacement);
    mylite_execution_session_scalar_cell_deinit(&value_cell);
    mylite_execution_session_scalar_cell_deinit(&replacement_cell);
    return rc;
}

static int evaluate_string_replace_scalar_argument(
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
    if (!string_replace_scalar_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "REPLACE() supports only string, integer, boolean, NULL, session scalar, "
            "and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "REPLACE() supports only string literals",
                "REPLACE() literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = mylite_execution_string_length_session_scalar_argument_value(
            database,
            expression,
            inout_cell
        );
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

static bool string_replace_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return mylite_execution_string_length_scalar_argument_is_admitted(expression);
}

static int evaluate_string_insert_scalar_argument(
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
    if (!string_replace_scalar_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "INSERT() supports only string, integer, boolean, NULL, session scalar, "
            "and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "INSERT() supports only string literals",
                "INSERT() literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = mylite_execution_string_length_session_scalar_argument_value(
            database,
            expression,
            inout_cell
        );
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

static int evaluate_string_insert_position_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_position,
    bool *out_is_null
) {
    if (out_position == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_position = 0;
    *out_is_null = false;

    if (!mylite_execution_string_slice_length_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "INSERT() position supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    return mylite_execution_string_slice_signed_integer_value(
        database,
        expression,
        "INSERT() position supports only integer, boolean, and NULL literals",
        "INSERT() position literals must fit the signed 64-bit range",
        out_position,
        out_is_null
    );
}

static int evaluate_string_insert_length_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_length,
    bool *out_is_null
) {
    if (out_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_length = 0;
    *out_is_null = false;

    if (!mylite_execution_string_slice_length_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "INSERT() length supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    return mylite_execution_string_slice_signed_integer_value(
        database,
        expression,
        "INSERT() length supports only integer, boolean, and NULL literals",
        "INSERT() length literals must fit the signed 64-bit range",
        out_length,
        out_is_null
    );
}

static int string_reverse_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell argument_cell = {0};
    char *owned_text = NULL;
    const char *text = NULL;
    size_t text_length = 0U;
    size_t result_length = 0U;
    bool is_null = false;
    bool helper_called = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_REVERSE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_native_function_parameter_count_error(database, "REVERSE");
        return MYLITE_ERROR;
    }

    rc = evaluate_string_reverse_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &argument_cell,
        &owned_text,
        &text,
        &text_length,
        &is_null
    );
    if (rc == MYLITE_OK && !is_null) {
        helper_called = true;
        rc = mylite_string_reverse_utf8_value(
            database,
            text,
            text_length,
            &out_cell->owned_text,
            &result_length
        );
        (void)result_length;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
    } else if (helper_called && rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(database, "invalid UTF-8 value in REVERSE()");
    } else if (out_cell->owned_text != NULL) {
        out_cell->value = out_cell->owned_text;
    }

    free(owned_text);
    mylite_execution_session_scalar_cell_deinit(&argument_cell);
    return rc;
}

static int evaluate_string_reverse_scalar_argument(
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
            "REVERSE() supports only string, integer, boolean, NULL, session scalar, "
            "and system variable arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return mylite_execution_set_unknown_column_reference_error(database, expression);
    }
    if (!string_reverse_scalar_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "REVERSE() supports only string, integer, boolean, NULL, session scalar, "
            "and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "REVERSE() supports only string literals",
                "REVERSE() literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = mylite_execution_string_length_session_scalar_argument_value(
            database,
            expression,
            inout_cell
        );
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

static bool string_reverse_scalar_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return mylite_execution_string_length_scalar_argument_is_admitted(expression);
}

static int soundex_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell argument_cell = {0};
    char *owned_text = NULL;
    const char *text = NULL;
    size_t text_length = 0U;
    size_t result_length = 0U;
    bool is_null = false;
    bool helper_called = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_SOUNDEX_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_native_function_parameter_count_error(database, "SOUNDEX");
        return MYLITE_ERROR;
    }

    rc = evaluate_soundex_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &argument_cell,
        &owned_text,
        &text,
        &text_length,
        &is_null
    );
    if (rc == MYLITE_OK && !is_null) {
        helper_called = true;
        rc = mylite_string_soundex_value(
            database,
            text,
            text_length,
            &out_cell->owned_text,
            &result_length
        );
        (void)result_length;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
    } else if (helper_called && rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(database, "invalid UTF-8 value in SOUNDEX()");
    } else if (out_cell->owned_text != NULL) {
        out_cell->value = out_cell->owned_text;
    }

    free(owned_text);
    mylite_execution_session_scalar_cell_deinit(&argument_cell);
    return rc;
}

static int evaluate_soundex_scalar_argument(
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
            "SOUNDEX() supports only string, integer, boolean, NULL, session scalar, "
            "and system variable arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return mylite_execution_set_unknown_column_reference_error(database, expression);
    }
    if (!soundex_scalar_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "SOUNDEX() supports only string, integer, boolean, NULL, session scalar, "
            "and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "SOUNDEX() supports only string literals",
                "SOUNDEX() literals do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = mylite_execution_string_length_session_scalar_argument_value(
            database,
            expression,
            inout_cell
        );
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

static bool soundex_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression) {
    return mylite_execution_string_length_scalar_argument_is_admitted(expression);
}

static int quote_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell argument_cell = {0};
    char *owned_text = NULL;
    const char *text = NULL;
    size_t text_length = 0U;
    size_t result_length = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_QUOTE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_native_function_parameter_count_error(database, "QUOTE");
        return MYLITE_ERROR;
    }

    rc = evaluate_quote_scalar_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &argument_cell,
        &owned_text,
        &text,
        &text_length,
        &is_null
    );
    if (rc == MYLITE_OK) {
        rc = mylite_string_quote_sql_value(
            database,
            text,
            text_length,
            is_null,
            &out_cell->owned_text,
            &result_length
        );
        (void)result_length;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
    } else if (rc == MYLITE_OK && out_cell->owned_text != NULL) {
        out_cell->value = out_cell->owned_text;
    }

    free(owned_text);
    mylite_execution_session_scalar_cell_deinit(&argument_cell);
    return rc;
}

static int evaluate_quote_scalar_argument(
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
            "QUOTE() supports only string, integer, DECIMAL, boolean, NULL, session scalar, "
            "and system variable arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return mylite_execution_set_unknown_column_reference_error(database, expression);
    }
    if (!quote_scalar_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "QUOTE() supports only string, integer, DECIMAL, boolean, NULL, session scalar, "
            "and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal_with_policy(
                database,
                expression,
                "QUOTE() supports only string literals",
                "QUOTE() string literal decoding failed",
                true,
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL) {
            *out_text = expression->span.text;
            *out_text_length = expression->span.length;
            return MYLITE_OK;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = mylite_execution_string_length_session_scalar_argument_value(
            database,
            expression,
            inout_cell
        );
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

static bool quote_scalar_argument_is_admitted(const struct mylite_sql_ast_node *expression) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION) {
        return false;
    }
    if (mylite_execution_is_session_scalar_expression(expression)) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal =
            mylite_execution_unwrap_parenthesized_expression(
                mylite_execution_child_at(expression, 0U)
            );

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        return (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
                mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) != 0;
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    return (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
            literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
            literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL ||
            literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
}

static int substring_index_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell value_cell = {0};
    struct session_scalar_cell delimiter_cell = {0};
    char *owned_value = NULL;
    char *owned_delimiter = NULL;
    const char *value = NULL;
    const char *delimiter = NULL;
    size_t value_length = 0U;
    size_t delimiter_length = 0U;
    size_t result_length = 0U;
    int64_t count = 0;
    bool value_is_null = false;
    bool delimiter_is_null = false;
    bool count_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_SUBSTRING_INDEX_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 3U) {
        mylite_execution_set_native_function_parameter_count_error(database, "SUBSTRING_INDEX");
        return MYLITE_ERROR;
    }

    rc = evaluate_substring_index_text_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &value_cell,
        &owned_value,
        &value,
        &value_length,
        &value_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_substring_index_text_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &delimiter_cell,
            &owned_delimiter,
            &delimiter,
            &delimiter_length,
            &delimiter_is_null
        );
    }
    if (rc == MYLITE_OK) {
        rc = evaluate_substring_index_count_argument(
            database,
            mylite_execution_child_at(expression, 2U),
            &count,
            &count_is_null
        );
    }
    if (rc == MYLITE_OK && !value_is_null && !delimiter_is_null && !count_is_null) {
        rc = mylite_string_substring_index_value(
            database,
            value,
            value_length,
            delimiter,
            delimiter_length,
            count,
            &out_cell->owned_text,
            &result_length
        );
        (void)result_length;
    }
    if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
    } else if (rc == MYLITE_OK && out_cell->owned_text != NULL) {
        out_cell->value = out_cell->owned_text;
    }

    free(owned_value);
    free(owned_delimiter);
    mylite_execution_session_scalar_cell_deinit(&value_cell);
    mylite_execution_session_scalar_cell_deinit(&delimiter_cell);
    return rc;
}

static int evaluate_substring_index_text_argument(
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
    if (!string_replace_scalar_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "SUBSTRING_INDEX() supports only string, integer, boolean, NULL, session scalar, "
            "and system variable value arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "SUBSTRING_INDEX() supports only string literals",
                "SUBSTRING_INDEX() arguments do not support NUL bytes",
                out_owned_text,
                out_text_length
            );
            if (rc == MYLITE_OK) {
                *out_text = *out_owned_text;
            }
            return rc;
        }
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL ||
        expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    } else {
        rc = mylite_execution_string_length_session_scalar_argument_value(
            database,
            expression,
            inout_cell
        );
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

static int evaluate_substring_index_count_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_count,
    bool *out_is_null
) {
    if (out_count == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_count = 0;
    *out_is_null = false;

    if (!mylite_execution_string_slice_length_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "SUBSTRING_INDEX() count supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    return mylite_execution_string_slice_signed_integer_value(
        database,
        expression,
        "SUBSTRING_INDEX() count supports only integer, boolean, and NULL literals",
        "SUBSTRING_INDEX() count literals must fit the signed 64-bit range",
        out_count,
        out_is_null
    );
}
