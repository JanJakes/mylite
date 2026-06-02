#include "mylite_execution_scalar_string_position.h"
#include "mylite_execution_scalar.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_string_bitmask.h"
#include "mylite_string_padding.h"
#include "mylite_string_search.h"

#include <mylite/mylite.h>

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct string_slice_right_bounds {
    size_t text_length;
    uint64_t requested_length;
    size_t character_count;
};

struct substring_text_bounds {
    const char *text;
    size_t text_length;
    int64_t position;
    bool has_length;
    int64_t requested_length;
};

struct string_bitmask_scalar_text_argument {
    struct mylite_string_bitmask_slice slice;
    struct session_scalar_cell cell;
    char *owned_text;
};

/* Static helper prototypes. */
static int string_slice_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_string_slice_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int evaluate_string_slice_length_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_length,
    bool *out_is_null
);
static int evaluate_string_slice_position_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_position,
    bool *out_is_null
);
static int slice_utf8_text_value(
    struct mylite_db *database,
    enum planned_string_slice_function_kind function_kind,
    const char *text,
    size_t text_length,
    int64_t requested_length,
    struct session_scalar_cell *out_cell
);
static int substring_utf8_text_value(
    struct mylite_db *database,
    const struct substring_text_bounds *bounds,
    struct session_scalar_cell *out_cell
);
static int string_slice_empty_result(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
);
static int find_left_slice_end(
    const char *text,
    size_t text_length,
    uint64_t requested_length,
    size_t *out_end
);
static int find_right_slice_start(
    const char *text,
    const struct string_slice_right_bounds *bounds,
    size_t *out_start
);
static bool string_slice_scalar_text_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
);
static bool string_slice_length_argument_is_admitted(const struct mylite_sql_ast_node *expression);
static int string_slice_signed_length_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_is_null
);
static int string_slice_signed_position_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_is_null
);
static int string_slice_signed_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_message,
    const char *range_message,
    int64_t *out_value,
    bool *out_is_null
);
static enum planned_string_slice_function_kind string_slice_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
static bool is_string_slice_function_kind(enum mylite_sql_ast_node_kind ast_kind);
static int string_padding_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_pad_string_padding_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum planned_string_padding_function_kind function_kind,
    char **out_result,
    size_t *out_result_length,
    bool *out_is_null
);
static int evaluate_repeat_string_padding_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_result,
    size_t *out_result_length,
    bool *out_is_null
);
static int evaluate_space_string_padding_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_result,
    size_t *out_result_length,
    bool *out_is_null
);
static int evaluate_string_padding_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int evaluate_string_padding_count_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_count,
    bool *out_is_null
);
static int string_padding_set_owned_result(
    struct mylite_db *database,
    int rc,
    char *value,
    size_t value_length,
    bool is_null,
    struct session_scalar_cell *out_cell
);
static enum planned_string_padding_function_kind string_padding_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
static enum mylite_string_padding_side string_padding_function_to_side(
    enum planned_string_padding_function_kind function_kind
);
static bool is_string_padding_function_kind(enum mylite_sql_ast_node_kind ast_kind);
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
static int string_search_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_string_search_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int evaluate_string_search_position_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_position,
    bool *out_is_null
);
static int format_string_search_result(
    struct mylite_db *database,
    int64_t result,
    struct session_scalar_cell *out_cell
);
static enum planned_string_search_function_kind string_search_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
static bool is_string_search_function_kind(enum mylite_sql_ast_node_kind ast_kind);
static int find_in_set_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int strcmp_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int evaluate_find_in_set_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int evaluate_strcmp_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);

int mylite_execution_scalar_string_slice_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return string_slice_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_string_padding_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return string_padding_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_string_bitmask_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return string_bitmask_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_string_search_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return string_search_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_find_in_set_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return find_in_set_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_strcmp_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return strcmp_function_value(database, expression, out_cell);
}

enum planned_string_slice_function_kind mylite_execution_string_slice_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return string_slice_function_kind(ast_kind);
}

bool mylite_execution_is_string_slice_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return is_string_slice_function_kind(ast_kind);
}

bool mylite_execution_string_slice_scalar_text_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return string_slice_scalar_text_argument_is_admitted(expression);
}

bool mylite_execution_string_slice_length_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return string_slice_length_argument_is_admitted(expression);
}

int mylite_execution_string_slice_signed_length_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_is_null
) {
    return string_slice_signed_length_value(database, expression, out_value, out_is_null);
}

int mylite_execution_string_slice_signed_position_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_is_null
) {
    return string_slice_signed_position_value(database, expression, out_value, out_is_null);
}

int mylite_execution_string_slice_signed_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_message,
    const char *range_message,
    int64_t *out_value,
    bool *out_is_null
) {
    return string_slice_signed_integer_value(
        database,
        expression,
        unsupported_message,
        range_message,
        out_value,
        out_is_null
    );
}

enum planned_string_padding_function_kind mylite_execution_string_padding_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return string_padding_function_kind(ast_kind);
}

bool mylite_execution_is_string_padding_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return is_string_padding_function_kind(ast_kind);
}

enum planned_string_bitmask_function_kind mylite_execution_string_bitmask_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return string_bitmask_function_kind(ast_kind);
}

bool mylite_execution_is_string_bitmask_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return is_string_bitmask_function_kind(ast_kind);
}

enum planned_string_search_function_kind mylite_execution_string_search_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return string_search_function_kind(ast_kind);
}

bool mylite_execution_is_string_search_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return is_string_search_function_kind(ast_kind);
}

static int string_slice_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum planned_string_slice_function_kind function_kind = PLANNED_STRING_SLICE_FUNCTION_NONE;
    struct session_scalar_cell argument_cell = {0};
    char *owned_text = NULL;
    const char *text = NULL;
    size_t text_length = 0U;
    int64_t position = 0;
    int64_t requested_length = 0;
    size_t argument_count = 0U;
    bool text_is_null = false;
    bool position_is_null = false;
    bool length_is_null = false;
    bool has_length = true;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_SLICE_FUNCTION_NONE
                                       : string_slice_function_kind(expression->kind);
    argument_count = expression == NULL ? 0U : mylite_sql_ast_node_child_count(expression);
    if (function_kind == PLANNED_STRING_SLICE_FUNCTION_NONE) {
        mylite_execution_set_unsupported_error(
            database,
            "string slice functions support exactly two arguments"
        );
        return MYLITE_ERROR;
    }
    if (function_kind == PLANNED_STRING_SLICE_FUNCTION_SUBSTRING) {
        has_length = argument_count == 3U;
        if (argument_count != 2U && argument_count != 3U) {
            mylite_execution_set_unsupported_error(
                database,
                "SUBSTRING functions support two or three arguments"
            );
            return MYLITE_ERROR;
        }
    } else if (argument_count != 2U) {
        mylite_execution_set_unsupported_error(
            database,
            "string slice functions support exactly two arguments"
        );
        return MYLITE_ERROR;
    }

    rc = evaluate_string_slice_text_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &argument_cell,
        &owned_text,
        &text,
        &text_length,
        &text_is_null
    );
    if (rc == MYLITE_OK && function_kind == PLANNED_STRING_SLICE_FUNCTION_SUBSTRING) {
        rc = evaluate_string_slice_position_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &position,
            &position_is_null
        );
    } else if (rc == MYLITE_OK) {
        rc = evaluate_string_slice_length_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &requested_length,
            &length_is_null
        );
    }
    if (rc == MYLITE_OK && function_kind == PLANNED_STRING_SLICE_FUNCTION_SUBSTRING && has_length) {
        rc = evaluate_string_slice_length_argument(
            database,
            mylite_execution_child_at(expression, 2U),
            &requested_length,
            &length_is_null
        );
    }
    if (rc != MYLITE_OK || text_is_null || position_is_null || length_is_null) {
        free(owned_text);
        mylite_execution_session_scalar_cell_deinit(&argument_cell);
        return rc;
    }

    if (function_kind == PLANNED_STRING_SLICE_FUNCTION_SUBSTRING) {
        struct substring_text_bounds substring_bounds = {
            .text = text,
            .text_length = text_length,
            .position = position,
            .has_length = has_length,
            .requested_length = requested_length,
        };

        rc = substring_utf8_text_value(database, &substring_bounds, out_cell);
    } else {
        rc = slice_utf8_text_value(
            database,
            function_kind,
            text,
            text_length,
            requested_length,
            out_cell
        );
    }
    free(owned_text);
    mylite_execution_session_scalar_cell_deinit(&argument_cell);
    return rc;
}

static int evaluate_string_slice_text_argument(
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
    if (!string_slice_scalar_text_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "string slice functions support only string, integer, boolean, NULL, "
            "session scalar, and system variable string arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "string slice functions support only string literals",
                "string slice function literals do not support NUL bytes",
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

static int evaluate_string_slice_length_argument(
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

    if (!string_slice_length_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "string slice functions support only integer, boolean, and NULL length literals"
        );
        return MYLITE_ERROR;
    }
    return string_slice_signed_length_value(database, expression, out_length, out_is_null);
}

static int evaluate_string_slice_position_argument(
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

    if (!string_slice_length_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "string slice functions support only integer, boolean, and NULL position literals"
        );
        return MYLITE_ERROR;
    }
    return string_slice_signed_position_value(database, expression, out_position, out_is_null);
}

static int slice_utf8_text_value(
    struct mylite_db *database,
    enum planned_string_slice_function_kind function_kind,
    const char *text,
    size_t text_length,
    int64_t requested_length,
    struct session_scalar_cell *out_cell
) {
    size_t character_count = 0U;
    size_t start = 0U;
    size_t end = 0U;
    size_t slice_length = 0U;
    char *value = NULL;
    int rc = MYLITE_OK;

    if (text == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (requested_length <= 0) {
        return string_slice_empty_result(database, out_cell);
    }

    rc = mylite_execution_validate_utf8_text(text, text_length, &character_count);
    if (rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(
            database,
            "invalid UTF-8 value in string slice function"
        );
        return MYLITE_ERROR;
    }

    if (function_kind == PLANNED_STRING_SLICE_FUNCTION_LEFT) {
        rc = find_left_slice_end(text, text_length, (uint64_t)requested_length, &end);
    } else if (function_kind == PLANNED_STRING_SLICE_FUNCTION_RIGHT) {
        struct string_slice_right_bounds bounds = {
            .text_length = text_length,
            .requested_length = (uint64_t)requested_length,
            .character_count = character_count,
        };

        end = text_length;
        rc = find_right_slice_start(text, &bounds, &start);
    } else {
        return MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(
            database,
            "invalid UTF-8 value in string slice function"
        );
        return MYLITE_ERROR;
    }
    slice_length = end - start;
    if (slice_length == SIZE_MAX) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    value = (char *)malloc(slice_length + 1U);
    if (value == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(value, text + start, slice_length);
    value[slice_length] = '\0';
    out_cell->owned_text = value;
    out_cell->value = value;
    return MYLITE_OK;
}

static int substring_utf8_text_value(
    struct mylite_db *database,
    const struct substring_text_bounds *bounds,
    struct session_scalar_cell *out_cell
) {
    uint64_t start_character = 0U;
    uint64_t end_character = 0U;
    uint64_t character_count64 = 0U;
    size_t character_count = 0U;
    size_t start = 0U;
    size_t end = 0U;
    size_t slice_length = 0U;
    char *value = NULL;
    int rc = MYLITE_OK;

    if (bounds == NULL || bounds->text == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (bounds->position == 0 || (bounds->has_length && bounds->requested_length <= 0)) {
        return string_slice_empty_result(database, out_cell);
    }

    rc = mylite_execution_validate_utf8_text(bounds->text, bounds->text_length, &character_count);
    if (rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(
            database,
            "invalid UTF-8 value in string slice function"
        );
        return MYLITE_ERROR;
    }
    character_count64 = (uint64_t)character_count;

    if (bounds->position > 0) {
        start_character = (uint64_t)bounds->position - 1U;
        if (start_character >= character_count64) {
            return string_slice_empty_result(database, out_cell);
        }
    } else {
        uint64_t magnitude = bounds->position == INT64_MIN ? (uint64_t)INT64_MAX + 1U
                                                           : (uint64_t)(-bounds->position);

        if (magnitude > character_count64) {
            return string_slice_empty_result(database, out_cell);
        }
        start_character = character_count64 - magnitude;
    }

    if (bounds->has_length) {
        uint64_t remaining = character_count64 - start_character;
        uint64_t requested = (uint64_t)bounds->requested_length;

        end_character = start_character + (requested > remaining ? remaining : requested);
    } else {
        end_character = character_count64;
    }

    rc = find_left_slice_end(bounds->text, bounds->text_length, start_character, &start);
    if (rc == MYLITE_OK) {
        rc = find_left_slice_end(bounds->text, bounds->text_length, end_character, &end);
    }
    if (rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(
            database,
            "invalid UTF-8 value in string slice function"
        );
        return MYLITE_ERROR;
    }

    slice_length = end - start;
    if (slice_length == SIZE_MAX) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    value = (char *)malloc(slice_length + 1U);
    if (value == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(value, bounds->text + start, slice_length);
    value[slice_length] = '\0';
    out_cell->owned_text = value;
    out_cell->value = value;
    return MYLITE_OK;
}

static int string_slice_empty_result(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell
) {
    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    out_cell->owned_text = (char *)malloc(1U);
    if (out_cell->owned_text == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_cell->owned_text[0] = '\0';
    out_cell->value = out_cell->owned_text;
    return MYLITE_OK;
}

static int find_left_slice_end(
    const char *text,
    size_t text_length,
    uint64_t requested_length,
    size_t *out_end
) {
    size_t index = 0U;
    uint64_t character_index = 0U;

    if (text == NULL || out_end == NULL) {
        return MYLITE_MISUSE;
    }
    while (index < text_length && character_index < requested_length) {
        size_t width = 0U;
        int rc = mylite_execution_utf8_sequence_width(text, text_length, index, &width);

        if (rc != MYLITE_OK) {
            return rc;
        }
        index += width;
        ++character_index;
    }
    *out_end = index;
    return MYLITE_OK;
}

static int find_right_slice_start(
    const char *text,
    const struct string_slice_right_bounds *bounds,
    size_t *out_start
) {
    size_t index = 0U;
    size_t skip_count = 0U;

    if (text == NULL || bounds == NULL || out_start == NULL) {
        return MYLITE_MISUSE;
    }
    if (bounds->requested_length >= bounds->character_count) {
        *out_start = 0U;
        return MYLITE_OK;
    }

    skip_count = bounds->character_count - (size_t)bounds->requested_length;
    for (size_t character_index = 0U; character_index < skip_count; ++character_index) {
        size_t width = 0U;
        int rc = mylite_execution_utf8_sequence_width(text, bounds->text_length, index, &width);

        if (rc != MYLITE_OK) {
            return rc;
        }
        index += width;
    }
    *out_start = index;
    return MYLITE_OK;
}

static bool string_slice_scalar_text_argument_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    return mylite_execution_string_length_scalar_argument_is_admitted(expression);
}

static bool string_slice_length_argument_is_admitted(const struct mylite_sql_ast_node *expression) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
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
    return (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
            literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_FALSE ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) != 0;
}

static int string_slice_signed_length_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_is_null
) {
    return string_slice_signed_integer_value(
        database,
        expression,
        "string slice functions support only integer, boolean, and NULL length literals",
        "string slice function length literals must fit the signed 64-bit range",
        out_value,
        out_is_null
    );
}

static int string_slice_signed_position_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_is_null
) {
    return string_slice_signed_integer_value(
        database,
        expression,
        "string slice functions support only integer, boolean, and NULL position literals",
        "string slice function position literals must fit the signed 64-bit range",
        out_value,
        out_is_null
    );
}

static int string_slice_signed_integer_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_message,
    const char *range_message,
    int64_t *out_value,
    bool *out_is_null
) {
    const struct mylite_sql_ast_node *literal =
        mylite_execution_unwrap_parenthesized_expression(expression);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool is_negative = false;
    uint64_t magnitude = 0U;

    if (out_value == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0;
    *out_is_null = false;

    if (literal != NULL && literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(literal);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        }
        literal = mylite_execution_unwrap_parenthesized_expression(
            mylite_execution_child_at(literal, 0U)
        );
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        mylite_execution_set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_value = 1;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_value = 0;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER ||
        mylite_execution_parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK ||
        (is_negative && magnitude > (uint64_t)INT64_MAX + 1U) ||
        (!is_negative && magnitude > (uint64_t)INT64_MAX)) {
        mylite_execution_set_unsupported_error(database, range_message);
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

static enum planned_string_slice_function_kind string_slice_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_LEFT_FUNCTION:
        return PLANNED_STRING_SLICE_FUNCTION_LEFT;
    case MYLITE_SQL_AST_RIGHT_FUNCTION:
        return PLANNED_STRING_SLICE_FUNCTION_RIGHT;
    case MYLITE_SQL_AST_SUBSTRING_FUNCTION:
    case MYLITE_SQL_AST_SUBSTR_FUNCTION:
    case MYLITE_SQL_AST_MID_FUNCTION:
        return PLANNED_STRING_SLICE_FUNCTION_SUBSTRING;
    default:
        return PLANNED_STRING_SLICE_FUNCTION_NONE;
    }
}

static bool is_string_slice_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return string_slice_function_kind(ast_kind) != PLANNED_STRING_SLICE_FUNCTION_NONE;
}

static int string_padding_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum planned_string_padding_function_kind function_kind = PLANNED_STRING_PADDING_FUNCTION_NONE;
    char *result = NULL;
    size_t result_length = 0U;
    bool result_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_PADDING_FUNCTION_NONE
                                       : string_padding_function_kind(expression->kind);
    switch (function_kind) {
    case PLANNED_STRING_PADDING_FUNCTION_LPAD:
    case PLANNED_STRING_PADDING_FUNCTION_RPAD:
        rc = evaluate_pad_string_padding_function(
            database,
            expression,
            function_kind,
            &result,
            &result_length,
            &result_is_null
        );
        break;
    case PLANNED_STRING_PADDING_FUNCTION_REPEAT:
        rc = evaluate_repeat_string_padding_function(
            database,
            expression,
            &result,
            &result_length,
            &result_is_null
        );
        break;
    case PLANNED_STRING_PADDING_FUNCTION_SPACE:
        rc = evaluate_space_string_padding_function(
            database,
            expression,
            &result,
            &result_length,
            &result_is_null
        );
        break;
    default:
        mylite_execution_set_unsupported_error(
            database,
            "string padding functions support LPAD, RPAD, REPEAT, and SPACE"
        );
        return MYLITE_ERROR;
    }

    if (rc == MYLITE_OK) {
        rc = string_padding_set_owned_result(
            database,
            rc,
            result,
            result_length,
            result_is_null,
            out_cell
        );
        result = NULL;
    } else if (rc == MYLITE_NOMEM) {
        mylite_execution_set_nomem_error(database);
    } else if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
        mylite_execution_set_runtime_error(
            database,
            "invalid UTF-8 value in string padding function"
        );
    }

    free(result);
    return rc;
}

static int evaluate_pad_string_padding_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum planned_string_padding_function_kind function_kind,
    char **out_result,
    size_t *out_result_length,
    bool *out_is_null
) {
    struct session_scalar_cell first_cell = {0};
    struct session_scalar_cell third_cell = {0};
    char *owned_first_text = NULL;
    char *owned_third_text = NULL;
    const char *first_text = NULL;
    const char *third_text = NULL;
    size_t first_length = 0U;
    size_t third_length = 0U;
    int64_t count = 0;
    size_t argument_count = 0U;
    bool first_is_null = false;
    bool count_is_null = false;
    bool third_is_null = false;
    int rc = MYLITE_OK;

    if (out_result == NULL || out_result_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    *out_result_length = 0U;
    *out_is_null = false;
    argument_count = expression == NULL ? 0U : mylite_sql_ast_node_child_count(expression);
    if (argument_count != 3U) {
        mylite_execution_set_native_function_parameter_count_error(
            database,
            function_kind == PLANNED_STRING_PADDING_FUNCTION_LPAD ? "LPAD" : "RPAD"
        );
        return MYLITE_ERROR;
    }

    rc = evaluate_string_padding_text_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &first_cell,
        &owned_first_text,
        &first_text,
        &first_length,
        &first_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_string_padding_count_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &count,
            &count_is_null
        );
    }
    if (rc == MYLITE_OK) {
        rc = evaluate_string_padding_text_argument(
            database,
            mylite_execution_child_at(expression, 2U),
            &third_cell,
            &owned_third_text,
            &third_text,
            &third_length,
            &third_is_null
        );
    }
    if (rc == MYLITE_OK && !first_is_null && !count_is_null && !third_is_null) {
        rc = mylite_string_pad_value(
            database,
            string_padding_function_to_side(function_kind),
            (struct mylite_string_padding_slice){
                .text = first_text,
                .length = first_length,
            },
            count,
            (struct mylite_string_padding_slice){
                .text = third_text,
                .length = third_length,
            },
            out_result,
            out_result_length,
            out_is_null
        );
    } else {
        *out_is_null = rc == MYLITE_OK;
    }

    free(owned_first_text);
    free(owned_third_text);
    mylite_execution_session_scalar_cell_deinit(&first_cell);
    mylite_execution_session_scalar_cell_deinit(&third_cell);
    return rc;
}

static int evaluate_repeat_string_padding_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_result,
    size_t *out_result_length,
    bool *out_is_null
) {
    struct session_scalar_cell first_cell = {0};
    char *owned_first_text = NULL;
    const char *first_text = NULL;
    size_t first_length = 0U;
    size_t argument_count = 0U;
    int64_t count = 0;
    bool first_is_null = false;
    bool count_is_null = false;
    int rc = MYLITE_OK;

    if (out_result == NULL || out_result_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    *out_result_length = 0U;
    *out_is_null = false;
    argument_count = expression == NULL ? 0U : mylite_sql_ast_node_child_count(expression);
    if (argument_count != 2U) {
        mylite_execution_set_native_function_parameter_count_error(database, "REPEAT");
        return MYLITE_ERROR;
    }

    rc = evaluate_string_padding_text_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &first_cell,
        &owned_first_text,
        &first_text,
        &first_length,
        &first_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_string_padding_count_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &count,
            &count_is_null
        );
    }
    if (rc == MYLITE_OK && !first_is_null && !count_is_null) {
        rc = mylite_string_repeat_value(
            database,
            (struct mylite_string_padding_slice){
                .text = first_text,
                .length = first_length,
            },
            count,
            out_result,
            out_result_length,
            out_is_null
        );
    } else {
        *out_is_null = rc == MYLITE_OK;
    }

    free(owned_first_text);
    mylite_execution_session_scalar_cell_deinit(&first_cell);
    return rc;
}

static int evaluate_space_string_padding_function(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_result,
    size_t *out_result_length,
    bool *out_is_null
) {
    int64_t count = 0;
    size_t argument_count = expression == NULL ? 0U : mylite_sql_ast_node_child_count(expression);
    bool count_is_null = false;
    int rc = MYLITE_OK;

    if (out_result == NULL || out_result_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    *out_result_length = 0U;
    *out_is_null = false;
    if (argument_count != 1U) {
        mylite_execution_set_native_function_parameter_count_error(database, "SPACE");
        return MYLITE_ERROR;
    }

    rc = evaluate_string_padding_count_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &count,
        &count_is_null
    );
    if (rc == MYLITE_OK && !count_is_null) {
        rc = mylite_string_space_value(count, out_result, out_result_length, out_is_null);
    } else {
        *out_is_null = rc == MYLITE_OK;
    }
    return rc;
}

static int evaluate_string_padding_text_argument(
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
    if (!string_slice_scalar_text_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "string padding functions support only string, integer, boolean, NULL, session "
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
                "string padding functions support only string literals",
                "string padding function literals do not support NUL bytes",
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

static int evaluate_string_padding_count_argument(
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

    if (!string_slice_length_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "string padding functions support only integer, boolean, and NULL length/count "
            "literals"
        );
        return MYLITE_ERROR;
    }
    return string_slice_signed_integer_value(
        database,
        expression,
        "string padding functions support only integer, boolean, and NULL length/count literals",
        "string padding function length/count literals must fit the signed 64-bit range",
        out_count,
        out_is_null
    );
}

static int string_padding_set_owned_result(
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
        mylite_execution_set_runtime_error(
            database,
            "invalid UTF-8 value in string padding function"
        );
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
            "invalid NUL byte in string padding function result"
        );
        return MYLITE_ERROR;
    }
    out_cell->owned_text = value;
    out_cell->value = value;
    return MYLITE_OK;
}

static enum planned_string_padding_function_kind string_padding_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_LPAD_FUNCTION:
        return PLANNED_STRING_PADDING_FUNCTION_LPAD;
    case MYLITE_SQL_AST_RPAD_FUNCTION:
        return PLANNED_STRING_PADDING_FUNCTION_RPAD;
    case MYLITE_SQL_AST_REPEAT_FUNCTION:
        return PLANNED_STRING_PADDING_FUNCTION_REPEAT;
    case MYLITE_SQL_AST_SPACE_FUNCTION:
        return PLANNED_STRING_PADDING_FUNCTION_SPACE;
    default:
        return PLANNED_STRING_PADDING_FUNCTION_NONE;
    }
}

static enum mylite_string_padding_side string_padding_function_to_side(
    enum planned_string_padding_function_kind function_kind
) {
    return function_kind == PLANNED_STRING_PADDING_FUNCTION_RPAD ? MYLITE_STRING_PADDING_RIGHT
                                                                 : MYLITE_STRING_PADDING_LEFT;
}

static bool is_string_padding_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return string_padding_function_kind(ast_kind) != PLANNED_STRING_PADDING_FUNCTION_NONE;
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
    if (!string_slice_length_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    rc = string_slice_signed_integer_value(
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
    if (!string_slice_scalar_text_argument_is_admitted(expression)) {
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

static int string_search_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum planned_string_search_function_kind function_kind = PLANNED_STRING_SEARCH_FUNCTION_NONE;
    struct session_scalar_cell first_cell = {0};
    struct session_scalar_cell second_cell = {0};
    char *owned_first_text = NULL;
    char *owned_second_text = NULL;
    const char *needle = NULL;
    const char *haystack = NULL;
    size_t needle_length = 0U;
    size_t haystack_length = 0U;
    int64_t position = 1;
    int64_t result = 0;
    size_t argument_count = 0U;
    bool first_is_null = false;
    bool second_is_null = false;
    bool position_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_STRING_SEARCH_FUNCTION_NONE
                                       : string_search_function_kind(expression->kind);
    argument_count = expression == NULL ? 0U : mylite_sql_ast_node_child_count(expression);
    if (function_kind == PLANNED_STRING_SEARCH_FUNCTION_NONE ||
        (argument_count != 2U && argument_count != 3U) ||
        (function_kind != PLANNED_STRING_SEARCH_FUNCTION_LOCATE && argument_count != 2U)) {
        mylite_execution_set_unsupported_error(
            database,
            "string search functions support LOCATE(substr,str[,pos]), INSTR(str,substr), "
            "and POSITION(substr IN str)"
        );
        return MYLITE_ERROR;
    }

    rc = evaluate_string_search_text_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &first_cell,
        &owned_first_text,
        &needle,
        &needle_length,
        &first_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_string_search_text_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &second_cell,
            &owned_second_text,
            &haystack,
            &haystack_length,
            &second_is_null
        );
    }
    if (rc == MYLITE_OK && argument_count == 3U) {
        rc = evaluate_string_search_position_argument(
            database,
            mylite_execution_child_at(expression, 2U),
            &position,
            &position_is_null
        );
    }
    if (rc != MYLITE_OK || first_is_null || second_is_null || position_is_null) {
        free(owned_first_text);
        free(owned_second_text);
        mylite_execution_session_scalar_cell_deinit(&first_cell);
        mylite_execution_session_scalar_cell_deinit(&second_cell);
        return rc;
    }

    if (function_kind == PLANNED_STRING_SEARCH_FUNCTION_INSTR) {
        const char *tmp_text = needle;
        size_t tmp_length = needle_length;

        needle = haystack;
        needle_length = haystack_length;
        haystack = tmp_text;
        haystack_length = tmp_length;
    }
    rc = mylite_string_search_locate_ascii_ci_value(
        database,
        needle,
        needle_length,
        haystack,
        haystack_length,
        position,
        &result
    );
    if (rc == MYLITE_OK) {
        rc = format_string_search_result(database, result, out_cell);
    }

    free(owned_first_text);
    free(owned_second_text);
    mylite_execution_session_scalar_cell_deinit(&first_cell);
    mylite_execution_session_scalar_cell_deinit(&second_cell);
    return rc;
}

static int evaluate_string_search_text_argument(
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
    if (!string_slice_scalar_text_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "string search functions support only string, integer, boolean, NULL, "
            "session scalar, and system variable arguments"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(expression);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = mylite_execution_decode_sql_string_literal(
                database,
                expression,
                "string search functions support only string literals",
                "string search function literals do not support NUL bytes",
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

static int evaluate_string_search_position_argument(
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

    if (!string_slice_length_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "LOCATE() position supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    return string_slice_signed_integer_value(
        database,
        expression,
        "LOCATE() position supports only integer, boolean, and NULL literals",
        "LOCATE() position literals must fit the signed 64-bit range",
        out_position,
        out_is_null
    );
}

static int format_string_search_result(
    struct mylite_db *database,
    int64_t result,
    struct session_scalar_cell *out_cell
) {
    int written = 0;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    written = snprintf(out_cell->integer_text, sizeof(out_cell->integer_text), "%" PRId64, result);
    if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
        mylite_execution_set_runtime_error(database, "failed to format string search result");
        return MYLITE_ERROR;
    }
    out_cell->value = out_cell->integer_text;
    return MYLITE_OK;
}

static enum planned_string_search_function_kind string_search_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_LOCATE_FUNCTION:
        return PLANNED_STRING_SEARCH_FUNCTION_LOCATE;
    case MYLITE_SQL_AST_INSTR_FUNCTION:
        return PLANNED_STRING_SEARCH_FUNCTION_INSTR;
    case MYLITE_SQL_AST_POSITION_FUNCTION:
        return PLANNED_STRING_SEARCH_FUNCTION_POSITION;
    default:
        return PLANNED_STRING_SEARCH_FUNCTION_NONE;
    }
}

static bool is_string_search_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return string_search_function_kind(ast_kind) != PLANNED_STRING_SEARCH_FUNCTION_NONE;
}

static int find_in_set_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell search_cell = {0};
    struct session_scalar_cell list_cell = {0};
    char *owned_search = NULL;
    char *owned_list = NULL;
    const char *search = NULL;
    const char *list = NULL;
    size_t search_length = 0U;
    size_t list_length = 0U;
    int64_t result = 0;
    bool search_is_null = false;
    bool list_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_FIND_IN_SET_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        mylite_execution_set_native_function_parameter_count_error(database, "FIND_IN_SET");
        return MYLITE_ERROR;
    }

    rc = evaluate_find_in_set_text_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &search_cell,
        &owned_search,
        &search,
        &search_length,
        &search_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_find_in_set_text_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &list_cell,
            &owned_list,
            &list,
            &list_length,
            &list_is_null
        );
    }
    if (rc != MYLITE_OK || search_is_null || list_is_null) {
        free(owned_search);
        free(owned_list);
        mylite_execution_session_scalar_cell_deinit(&search_cell);
        mylite_execution_session_scalar_cell_deinit(&list_cell);
        return rc;
    }

    rc = mylite_string_search_find_in_set_ascii_ci_value(
        database,
        search,
        search_length,
        list,
        list_length,
        &result
    );
    if (rc == MYLITE_OK) {
        rc = format_string_search_result(database, result, out_cell);
    }

    free(owned_search);
    free(owned_list);
    mylite_execution_session_scalar_cell_deinit(&search_cell);
    mylite_execution_session_scalar_cell_deinit(&list_cell);
    return rc;
}

static int strcmp_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct session_scalar_cell left_cell = {0};
    struct session_scalar_cell right_cell = {0};
    char *owned_left = NULL;
    char *owned_right = NULL;
    const char *left = NULL;
    const char *right = NULL;
    size_t left_length = 0U;
    size_t right_length = 0U;
    int64_t result = 0;
    bool left_is_null = false;
    bool right_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_STRCMP_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        mylite_execution_set_native_function_parameter_count_error(database, "STRCMP");
        return MYLITE_ERROR;
    }

    rc = evaluate_strcmp_text_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        &left_cell,
        &owned_left,
        &left,
        &left_length,
        &left_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_strcmp_text_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            &right_cell,
            &owned_right,
            &right,
            &right_length,
            &right_is_null
        );
    }
    if (rc != MYLITE_OK || left_is_null || right_is_null) {
        free(owned_left);
        free(owned_right);
        mylite_execution_session_scalar_cell_deinit(&left_cell);
        mylite_execution_session_scalar_cell_deinit(&right_cell);
        return rc;
    }

    rc = mylite_string_search_strcmp_ascii_ci_value(
        database,
        left,
        left_length,
        right,
        right_length,
        &result
    );
    if (rc == MYLITE_OK) {
        rc = format_string_search_result(database, result, out_cell);
    }

    free(owned_left);
    free(owned_right);
    mylite_execution_session_scalar_cell_deinit(&left_cell);
    mylite_execution_session_scalar_cell_deinit(&right_cell);
    return rc;
}

static int evaluate_find_in_set_text_argument(
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
    if (!string_slice_scalar_text_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "FIND_IN_SET() supports only string, integer, boolean, NULL, session scalar, "
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
                "FIND_IN_SET() supports only string literals",
                "FIND_IN_SET() arguments do not support NUL bytes",
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

static int evaluate_strcmp_text_argument(
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
    if (!string_slice_scalar_text_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(
            database,
            "STRCMP() supports only string, integer, boolean, NULL, session scalar, "
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
                "STRCMP() supports only string literals",
                "STRCMP() arguments do not support NUL bytes",
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
