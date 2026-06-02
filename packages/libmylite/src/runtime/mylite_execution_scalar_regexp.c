#include "mylite_execution_scalar_regexp.h"
#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_string_position.h"

#include "mylite_regexp.h"

#include <mylite/mylite.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct regexp_string_function_call_shape {
    enum planned_regexp_string_function_kind kind;
    size_t child_count;
};

/* Static helper prototypes. */
static int regexp_like_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int regexp_string_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int validate_regexp_string_function_argument_count(
    struct mylite_db *database,
    const struct regexp_string_function_call_shape *shape
);
static enum planned_regexp_string_function_kind regexp_string_function_kind(
    enum mylite_sql_ast_node_kind kind
);
static const char *regexp_string_function_name(enum planned_regexp_string_function_kind kind);
static const char *regexp_string_function_argument_count_error_name(
    enum mylite_sql_ast_node_kind kind
);
static const struct mylite_sql_ast_node *regexp_string_function_arguments(
    const struct mylite_sql_ast_node *expression
);
static size_t regexp_string_function_argument_count(const struct mylite_sql_ast_node *expression);
static const struct mylite_sql_ast_node *regexp_string_function_argument_at(
    const struct mylite_sql_ast_node *expression,
    size_t index
);
static int evaluate_regexp_like_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool allow_session_scalar,
    const struct regexp_like_text_argument_messages *messages,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static bool regexp_like_literal_or_unary_expression_is_admitted(
    const struct mylite_sql_ast_node *expression
);
static int evaluate_regexp_like_literal_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct regexp_like_text_argument_messages *messages,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int regexp_like_cell_text_result(
    struct session_scalar_cell *inout_cell,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int evaluate_regexp_like_match_type_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_is_null,
    bool *out_case_sensitive
);
static int regexp_like_case_sensitive_from_match_type(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    bool *out_case_sensitive
);
static int validate_regexp_like_pattern(
    struct mylite_db *database,
    const char *pattern,
    size_t pattern_length,
    bool case_sensitive,
    const char *unsupported_message
);
static int set_regexp_like_compile_error(
    struct mylite_db *database,
    enum mylite_regexp_compile_status status,
    const char *unsupported_message
);
static int match_regexp_like_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    const char *pattern,
    size_t pattern_length,
    bool case_sensitive,
    bool *out_matches
);
static int regexp_string_find_match(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    const char *pattern,
    size_t pattern_length,
    struct mylite_regexp_match *out_match
);
static int regexp_string_compile_error(
    struct mylite_db *database,
    enum mylite_regexp_compile_status status
);
static int regexp_string_match_error(
    struct mylite_db *database,
    enum mylite_regexp_match_status status
);
static int regexp_string_result_value(
    struct mylite_db *database,
    enum planned_regexp_string_function_kind kind,
    const char *value,
    size_t value_length,
    bool value_is_null,
    const char *pattern,
    size_t pattern_length,
    bool pattern_is_null,
    const char *replacement,
    size_t replacement_length,
    bool replacement_is_null,
    struct session_scalar_cell *out_cell
);
static int regexp_instr_or_substr_result_value(
    struct mylite_db *database,
    enum planned_regexp_string_function_kind kind,
    const char *value,
    size_t value_length,
    const char *pattern,
    size_t pattern_length,
    struct session_scalar_cell *out_cell
);
static int regexp_substr_result_value(
    struct mylite_db *database,
    const char *value,
    const struct mylite_regexp_match *match,
    struct session_scalar_cell *out_cell
);
static int regexp_replace_result_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    const char *pattern,
    size_t pattern_length,
    const char *replacement,
    size_t replacement_length,
    struct session_scalar_cell *out_cell
);
static int regexp_replace_append(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell,
    const char *text,
    size_t text_length
);

int mylite_execution_scalar_regexp_like_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return regexp_like_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_regexp_string_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return regexp_string_function_value(database, expression, out_cell);
}

int mylite_execution_evaluate_regexp_like_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool allow_session_scalar,
    const struct regexp_like_text_argument_messages *messages,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    return evaluate_regexp_like_text_argument(
        database,
        expression,
        allow_session_scalar,
        messages,
        inout_cell,
        out_owned_text,
        out_text,
        out_text_length,
        out_is_null
    );
}

int mylite_execution_evaluate_regexp_like_match_type_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_is_null,
    bool *out_case_sensitive
) {
    return evaluate_regexp_like_match_type_argument(
        database,
        expression,
        out_is_null,
        out_case_sensitive
    );
}

int mylite_execution_validate_regexp_like_pattern(
    struct mylite_db *database,
    const char *pattern,
    size_t pattern_length,
    bool case_sensitive,
    const char *unsupported_message
) {
    return validate_regexp_like_pattern(
        database,
        pattern,
        pattern_length,
        case_sensitive,
        unsupported_message
    );
}

enum planned_regexp_string_function_kind mylite_execution_regexp_string_function_kind(
    enum mylite_sql_ast_node_kind kind
) {
    return regexp_string_function_kind(kind);
}

const char *mylite_execution_regexp_string_function_name(
    enum planned_regexp_string_function_kind kind
) {
    return regexp_string_function_name(kind);
}

const char *mylite_execution_regexp_string_function_argument_count_error_name(
    enum mylite_sql_ast_node_kind kind
) {
    return regexp_string_function_argument_count_error_name(kind);
}

size_t mylite_execution_regexp_string_function_argument_count(
    const struct mylite_sql_ast_node *expression
) {
    return regexp_string_function_argument_count(expression);
}

const struct mylite_sql_ast_node *mylite_execution_regexp_string_function_argument_at(
    const struct mylite_sql_ast_node *expression,
    size_t index
) {
    return regexp_string_function_argument_at(expression, index);
}

static int regexp_like_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    static const struct regexp_like_text_argument_messages value_messages = {
        .unsupported = "REGEXP_LIKE() supports only string, integer, boolean, NULL, session "
                       "scalar, and system "
                       "variable value arguments",
        .string_unsupported = "REGEXP_LIKE() supports only string value literals",
        .embedded_nul = "REGEXP_LIKE() value arguments do not support NUL bytes",
        .non_ascii = "REGEXP_LIKE() arguments support only ASCII text",
    };
    static const struct regexp_like_text_argument_messages pattern_messages = {
        .unsupported =
            "REGEXP_LIKE() supports only string, integer, boolean, and NULL pattern arguments",
        .string_unsupported = "REGEXP_LIKE() supports only string pattern literals",
        .embedded_nul = "REGEXP_LIKE() pattern arguments do not support NUL bytes",
        .non_ascii = "REGEXP_LIKE() arguments support only ASCII text",
    };
    struct session_scalar_cell value_cell = {0};
    struct session_scalar_cell pattern_cell = {0};
    char *owned_value = NULL;
    char *owned_pattern = NULL;
    const char *value = NULL;
    const char *pattern = NULL;
    size_t value_length = 0U;
    size_t pattern_length = 0U;
    bool value_is_null = false;
    bool pattern_is_null = false;
    bool match_type_is_null = false;
    bool case_sensitive = false;
    bool matches = false;
    size_t child_count = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_REGEXP_LIKE_FUNCTION) {
        mylite_execution_set_native_function_parameter_count_error(database, "REGEXP_LIKE");
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count != 2U && child_count != 3U) {
        mylite_execution_set_native_function_parameter_count_error(database, "REGEXP_LIKE");
        return MYLITE_ERROR;
    }

    if (child_count == 3U) {
        rc = evaluate_regexp_like_match_type_argument(
            database,
            mylite_execution_child_at(expression, 2U),
            &match_type_is_null,
            &case_sensitive
        );
        if (rc != MYLITE_OK || match_type_is_null) {
            return rc;
        }
    }

    rc = evaluate_regexp_like_text_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        true,
        &value_messages,
        &value_cell,
        &owned_value,
        &value,
        &value_length,
        &value_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_regexp_like_text_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            false,
            &pattern_messages,
            &pattern_cell,
            &owned_pattern,
            &pattern,
            &pattern_length,
            &pattern_is_null
        );
    }
    if (rc == MYLITE_OK && !pattern_is_null) {
        rc = validate_regexp_like_pattern(
            database,
            pattern,
            pattern_length,
            case_sensitive,
            "REGEXP_LIKE() patterns support only MyLite's baseline ASCII regular expression subset"
        );
    }
    if (rc == MYLITE_OK && !value_is_null && !pattern_is_null) {
        rc = match_regexp_like_value(
            database,
            value,
            value_length,
            pattern,
            pattern_length,
            case_sensitive,
            &matches
        );
    }
    if (rc == MYLITE_OK && !value_is_null && !pattern_is_null) {
        uint64_t match_value = 0U;

        if (matches) {
            match_value = 1U;
        }
        rc = mylite_execution_format_session_scalar_uint64_value(database, match_value, out_cell);
    }

    free(owned_value);
    free(owned_pattern);
    mylite_execution_session_scalar_cell_deinit(&value_cell);
    mylite_execution_session_scalar_cell_deinit(&pattern_cell);
    return rc;
}

static int regexp_string_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    static const struct regexp_like_text_argument_messages value_messages = {
        .unsupported = "REGEXP string functions support only string, integer, boolean, NULL, "
                       "session scalar, and system variable value arguments",
        .string_unsupported = "REGEXP string functions support only string value literals",
        .embedded_nul = "REGEXP string function value arguments do not support NUL bytes",
        .non_ascii = "REGEXP string function arguments support only ASCII text",
    };
    static const struct regexp_like_text_argument_messages pattern_messages = {
        .unsupported =
            "REGEXP string functions support only string, integer, boolean, and NULL patterns",
        .string_unsupported = "REGEXP string functions support only string pattern literals",
        .embedded_nul = "REGEXP string function pattern arguments do not support NUL bytes",
        .non_ascii = "REGEXP string function arguments support only ASCII text",
    };
    static const struct regexp_like_text_argument_messages replacement_messages = {
        .unsupported = "REGEXP_REPLACE() supports only string, integer, boolean, NULL, session "
                       "scalar, and system variable replacement arguments",
        .string_unsupported = "REGEXP_REPLACE() supports only string replacement literals",
        .embedded_nul = "REGEXP_REPLACE() replacement arguments do not support NUL bytes",
        .non_ascii = "REGEXP_REPLACE() arguments support only ASCII text",
    };
    struct session_scalar_cell value_cell = {0};
    struct session_scalar_cell pattern_cell = {0};
    struct session_scalar_cell replacement_cell = {0};
    char *owned_value = NULL;
    char *owned_pattern = NULL;
    char *owned_replacement = NULL;
    const char *value = NULL;
    const char *pattern = NULL;
    const char *replacement = NULL;
    size_t value_length = 0U;
    size_t pattern_length = 0U;
    size_t replacement_length = 0U;
    bool value_is_null = false;
    bool pattern_is_null = false;
    bool replacement_is_null = false;
    struct regexp_string_function_call_shape shape = {
        .kind = PLANNED_REGEXP_STRING_FUNCTION_NONE,
        .child_count = 0U,
    };
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_parse_error(database);
        return MYLITE_ERROR;
    }
    shape.kind = regexp_string_function_kind(expression->kind);
    shape.child_count = regexp_string_function_argument_count(expression);
    rc = validate_regexp_string_function_argument_count(database, &shape);
    if (rc != MYLITE_OK) {
        return MYLITE_ERROR;
    }

    rc = evaluate_regexp_like_text_argument(
        database,
        regexp_string_function_argument_at(expression, 0U),
        true,
        &value_messages,
        &value_cell,
        &owned_value,
        &value,
        &value_length,
        &value_is_null
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_regexp_like_text_argument(
            database,
            regexp_string_function_argument_at(expression, 1U),
            false,
            &pattern_messages,
            &pattern_cell,
            &owned_pattern,
            &pattern,
            &pattern_length,
            &pattern_is_null
        );
    }
    if (rc == MYLITE_OK && shape.kind == PLANNED_REGEXP_STRING_FUNCTION_REPLACE) {
        rc = evaluate_regexp_like_text_argument(
            database,
            regexp_string_function_argument_at(expression, 2U),
            true,
            &replacement_messages,
            &replacement_cell,
            &owned_replacement,
            &replacement,
            &replacement_length,
            &replacement_is_null
        );
    }
    if (rc == MYLITE_OK) {
        rc = regexp_string_result_value(
            database,
            shape.kind,
            value,
            value_length,
            value_is_null,
            pattern,
            pattern_length,
            pattern_is_null,
            replacement,
            replacement_length,
            replacement_is_null,
            out_cell
        );
    }

    free(owned_value);
    free(owned_pattern);
    free(owned_replacement);
    mylite_execution_session_scalar_cell_deinit(&value_cell);
    mylite_execution_session_scalar_cell_deinit(&pattern_cell);
    mylite_execution_session_scalar_cell_deinit(&replacement_cell);
    return rc;
}

static int validate_regexp_string_function_argument_count(
    struct mylite_db *database,
    const struct regexp_string_function_call_shape *shape
) {
    if (shape->kind == PLANNED_REGEXP_STRING_FUNCTION_INSTR ||
        shape->kind == PLANNED_REGEXP_STRING_FUNCTION_SUBSTR) {
        if (shape->child_count == 2U) {
            return MYLITE_OK;
        }
        if (shape->child_count > 2U) {
            mylite_execution_set_unsupported_error(
                database,
                "REGEXP_INSTR() and REGEXP_SUBSTR() optional arguments are not supported"
            );
        } else {
            mylite_execution_set_native_function_parameter_count_error(
                database,
                regexp_string_function_name(shape->kind)
            );
        }
        return MYLITE_ERROR;
    }
    if (shape->kind == PLANNED_REGEXP_STRING_FUNCTION_REPLACE) {
        if (shape->child_count == 3U) {
            return MYLITE_OK;
        }
        if (shape->child_count > 3U) {
            mylite_execution_set_unsupported_error(
                database,
                "REGEXP_REPLACE() optional arguments are not supported"
            );
        } else {
            mylite_execution_set_native_function_parameter_count_error(database, "REGEXP_REPLACE");
        }
        return MYLITE_ERROR;
    }

    mylite_execution_set_parse_error(database);
    return MYLITE_ERROR;
}

static enum planned_regexp_string_function_kind regexp_string_function_kind(
    enum mylite_sql_ast_node_kind kind
) {
    switch (kind) {
    case MYLITE_SQL_AST_REGEXP_INSTR_FUNCTION:
        return PLANNED_REGEXP_STRING_FUNCTION_INSTR;
    case MYLITE_SQL_AST_REGEXP_SUBSTR_FUNCTION:
        return PLANNED_REGEXP_STRING_FUNCTION_SUBSTR;
    case MYLITE_SQL_AST_REGEXP_REPLACE_FUNCTION:
        return PLANNED_REGEXP_STRING_FUNCTION_REPLACE;
    default:
        return PLANNED_REGEXP_STRING_FUNCTION_NONE;
    }
}

static const char *regexp_string_function_name(enum planned_regexp_string_function_kind kind) {
    switch (kind) {
    case PLANNED_REGEXP_STRING_FUNCTION_INSTR:
        return "REGEXP_INSTR";
    case PLANNED_REGEXP_STRING_FUNCTION_SUBSTR:
        return "REGEXP_SUBSTR";
    case PLANNED_REGEXP_STRING_FUNCTION_REPLACE:
        return "REGEXP_REPLACE";
    case PLANNED_REGEXP_STRING_FUNCTION_NONE:
        break;
    }
    return "REGEXP";
}

static const char *regexp_string_function_argument_count_error_name(
    enum mylite_sql_ast_node_kind kind
) {
    switch (kind) {
    case MYLITE_SQL_AST_REGEXP_INSTR_ARGUMENT_COUNT_ERROR:
        return "REGEXP_INSTR";
    case MYLITE_SQL_AST_REGEXP_SUBSTR_ARGUMENT_COUNT_ERROR:
        return "REGEXP_SUBSTR";
    case MYLITE_SQL_AST_REGEXP_REPLACE_ARGUMENT_COUNT_ERROR:
        return "REGEXP_REPLACE";
    default:
        return NULL;
    }
}

static const struct mylite_sql_ast_node *regexp_string_function_arguments(
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node *arguments = NULL;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return NULL;
    }
    if (mylite_sql_ast_node_child_count(expression) == 1U) {
        arguments = mylite_execution_child_at(expression, 0U);
        if (arguments != NULL && arguments->kind == MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
            return arguments;
        }
    }
    return expression;
}

static size_t regexp_string_function_argument_count(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *arguments = regexp_string_function_arguments(expression);

    if (arguments == NULL) {
        return 0U;
    }
    return mylite_sql_ast_node_child_count(arguments);
}

static const struct mylite_sql_ast_node *regexp_string_function_argument_at(
    const struct mylite_sql_ast_node *expression,
    size_t index
) {
    return mylite_execution_child_at(regexp_string_function_arguments(expression), index);
}

static int evaluate_regexp_like_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool allow_session_scalar,
    const struct regexp_like_text_argument_messages *messages,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    int rc = MYLITE_OK;

    if (messages == NULL || inout_cell == NULL || out_owned_text == NULL || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_owned_text = NULL;
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (!mylite_execution_string_slice_scalar_text_argument_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }
    if (!allow_session_scalar && !regexp_like_literal_or_unary_expression_is_admitted(expression)) {
        mylite_execution_set_unsupported_error(database, messages->unsupported);
        return MYLITE_ERROR;
    }

    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        rc = evaluate_regexp_like_literal_text_argument(
            database,
            expression,
            messages,
            inout_cell,
            out_owned_text,
            out_text,
            out_text_length,
            out_is_null
        );
    } else if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
        if (rc == MYLITE_OK) {
            rc = regexp_like_cell_text_result(inout_cell, out_text, out_text_length, out_is_null);
        }
    } else {
        rc = mylite_execution_string_length_session_scalar_argument_value(
            database,
            expression,
            inout_cell
        );
        if (rc == MYLITE_OK) {
            rc = regexp_like_cell_text_result(inout_cell, out_text, out_text_length, out_is_null);
        }
    }
    if (rc != MYLITE_OK || *out_is_null) {
        return rc;
    }
    if (!mylite_execution_text_value_is_supported_string_key(*out_text, *out_text_length)) {
        mylite_execution_set_unsupported_error(database, messages->non_ascii);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static bool regexp_like_literal_or_unary_expression_is_admitted(
    const struct mylite_sql_ast_node *expression
) {
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return true;
    }
    return false;
}

static int evaluate_regexp_like_literal_text_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct regexp_like_text_argument_messages *messages,
    struct session_scalar_cell *inout_cell,
    char **out_owned_text,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    enum mylite_sql_ast_literal_kind literal_kind = mylite_sql_ast_node_literal_kind(expression);
    int rc = MYLITE_OK;

    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        rc = mylite_execution_decode_sql_string_literal(
            database,
            expression,
            messages->string_unsupported,
            messages->embedded_nul,
            out_owned_text,
            out_text_length
        );
        if (rc == MYLITE_OK) {
            *out_text = *out_owned_text;
        }
        return rc;
    }

    rc = mylite_execution_literal_projection_value(database, expression, inout_cell);
    if (rc == MYLITE_OK) {
        rc = regexp_like_cell_text_result(inout_cell, out_text, out_text_length, out_is_null);
    }
    return rc;
}

static int regexp_like_cell_text_result(
    struct session_scalar_cell *inout_cell,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    if (inout_cell->value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    *out_text = inout_cell->value;
    *out_text_length = strlen(inout_cell->value);
    return MYLITE_OK;
}

static int evaluate_regexp_like_match_type_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_is_null,
    bool *out_case_sensitive
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (out_is_null == NULL || out_case_sensitive == NULL) {
        return MYLITE_MISUSE;
    }
    *out_is_null = false;
    *out_case_sensitive = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        mylite_execution_set_unsupported_error(
            database,
            "REGEXP_LIKE() match_type supports only string and NULL literals"
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
            "REGEXP_LIKE() match_type supports only string and NULL literals"
        );
        return MYLITE_ERROR;
    }

    rc = mylite_execution_decode_sql_string_literal(
        database,
        expression,
        "REGEXP_LIKE() match_type supports only string literals",
        "REGEXP_LIKE() match_type literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK) {
        rc = regexp_like_case_sensitive_from_match_type(
            database,
            text,
            text_length,
            out_case_sensitive
        );
    }

    free(text);
    return rc;
}

static int regexp_like_case_sensitive_from_match_type(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    bool *out_case_sensitive
) {
    if (text == NULL || out_case_sensitive == NULL) {
        return MYLITE_MISUSE;
    }
    *out_case_sensitive = false;
    for (size_t index = 0U; index < text_length; ++index) {
        if (text[index] == 'c') {
            *out_case_sensitive = true;
        } else if (text[index] == 'i') {
            *out_case_sensitive = false;
        } else {
            mylite_execution_set_unsupported_error(
                database,
                "REGEXP_LIKE() match_type supports only c and i flags"
            );
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

static int validate_regexp_like_pattern(
    struct mylite_db *database,
    const char *pattern,
    size_t pattern_length,
    bool case_sensitive,
    const char *unsupported_message
) {
    struct mylite_regexp_program *program = NULL;
    enum mylite_regexp_compile_status status = MYLITE_REGEXP_COMPILE_OK;

    if (!mylite_execution_text_value_is_supported_string_key(pattern, pattern_length)) {
        mylite_execution_set_unsupported_error(
            database,
            "REGEXP_LIKE() pattern arguments support only ASCII text"
        );
        return MYLITE_ERROR;
    }
    if (case_sensitive) {
        status = mylite_regexp_compile_ascii_cs(pattern, pattern_length, &program);
    } else {
        status = mylite_regexp_compile_ascii_ci(pattern, pattern_length, &program);
    }
    mylite_regexp_program_free(program);
    if (status != MYLITE_REGEXP_COMPILE_OK) {
        return set_regexp_like_compile_error(database, status, unsupported_message);
    }
    return MYLITE_OK;
}

static int set_regexp_like_compile_error(
    struct mylite_db *database,
    enum mylite_regexp_compile_status status,
    const char *unsupported_message
) {
    if (status == MYLITE_REGEXP_COMPILE_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (status == MYLITE_REGEXP_COMPILE_UNCLOSED_BRACKET) {
        mylite_execution_set_regexp_error(
            database,
            "The regular expression contains an unclosed bracket expression."
        );
        return MYLITE_ERROR;
    }
    if (status == MYLITE_REGEXP_COMPILE_INVALID_RANGE) {
        mylite_execution_set_regexp_character_range_error(
            database,
            "The regular expression contains an invalid character range."
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_unsupported_error(database, unsupported_message);
    return MYLITE_ERROR;
}

static int match_regexp_like_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    const char *pattern,
    size_t pattern_length,
    bool case_sensitive,
    bool *out_matches
) {
    struct mylite_regexp_program *program = NULL;
    enum mylite_regexp_compile_status compile_status = MYLITE_REGEXP_COMPILE_OK;
    enum mylite_regexp_match_status match_status = MYLITE_REGEXP_MATCH_OK;

    if (out_matches == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matches = false;
    if (case_sensitive) {
        compile_status = mylite_regexp_compile_ascii_cs(pattern, pattern_length, &program);
    } else {
        compile_status = mylite_regexp_compile_ascii_ci(pattern, pattern_length, &program);
    }
    if (compile_status != MYLITE_REGEXP_COMPILE_OK) {
        mylite_regexp_program_free(program);
        return set_regexp_like_compile_error(
            database,
            compile_status,
            "REGEXP_LIKE() patterns support only MyLite's baseline ASCII regular expression subset"
        );
    }
    if (case_sensitive) {
        match_status =
            mylite_regexp_program_match_ascii_cs(program, value, value_length, out_matches);
    } else {
        match_status =
            mylite_regexp_program_match_ascii_ci(program, value, value_length, out_matches);
    }
    mylite_regexp_program_free(program);
    if (match_status == MYLITE_REGEXP_MATCH_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (match_status == MYLITE_REGEXP_MATCH_UNSUPPORTED_VALUE) {
        mylite_execution_set_unsupported_error(
            database,
            "REGEXP_LIKE() value arguments support only ASCII text"
        );
        return MYLITE_ERROR;
    }
    if (match_status != MYLITE_REGEXP_MATCH_OK) {
        mylite_execution_set_unsupported_error(
            database,
            "REGEXP_LIKE() value arguments are too large"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int regexp_string_find_match(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    const char *pattern,
    size_t pattern_length,
    struct mylite_regexp_match *out_match
) {
    struct mylite_regexp_program *program = NULL;
    enum mylite_regexp_compile_status compile_status = MYLITE_REGEXP_COMPILE_OK;
    enum mylite_regexp_match_status match_status = MYLITE_REGEXP_MATCH_OK;

    if (out_match == NULL) {
        return MYLITE_MISUSE;
    }
    *out_match = (struct mylite_regexp_match){
        .matched = false,
        .start = 0U,
        .end = 0U,
    };
    compile_status = mylite_regexp_compile_ascii_ci(pattern, pattern_length, &program);
    if (compile_status != MYLITE_REGEXP_COMPILE_OK) {
        mylite_regexp_program_free(program);
        return regexp_string_compile_error(database, compile_status);
    }

    match_status = mylite_regexp_program_find_ascii_ci(program, value, value_length, 0U, out_match);
    mylite_regexp_program_free(program);
    if (match_status != MYLITE_REGEXP_MATCH_OK) {
        return regexp_string_match_error(database, match_status);
    }
    return MYLITE_OK;
}

static int regexp_string_compile_error(
    struct mylite_db *database,
    enum mylite_regexp_compile_status status
) {
    if (status == MYLITE_REGEXP_COMPILE_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (status == MYLITE_REGEXP_COMPILE_UNCLOSED_BRACKET) {
        mylite_execution_set_regexp_error(
            database,
            "The regular expression contains an unclosed bracket expression."
        );
        return MYLITE_ERROR;
    }
    if (status == MYLITE_REGEXP_COMPILE_INVALID_RANGE) {
        mylite_execution_set_regexp_character_range_error(
            database,
            "The regular expression contains an invalid character range."
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_unsupported_error(
        database,
        "REGEXP string function patterns support only MyLite's baseline ASCII regular expression "
        "subset"
    );
    return MYLITE_ERROR;
}

static int regexp_string_match_error(
    struct mylite_db *database,
    enum mylite_regexp_match_status status
) {
    if (status == MYLITE_REGEXP_MATCH_NOMEM) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (status == MYLITE_REGEXP_MATCH_UNSUPPORTED_VALUE) {
        mylite_execution_set_unsupported_error(
            database,
            "REGEXP string function values support only ASCII text"
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_unsupported_error(database, "REGEXP string function values are too large");
    return MYLITE_ERROR;
}

static int regexp_string_result_value(
    struct mylite_db *database,
    enum planned_regexp_string_function_kind kind,
    const char *value,
    size_t value_length,
    bool value_is_null,
    const char *pattern,
    size_t pattern_length,
    bool pattern_is_null,
    const char *replacement,
    size_t replacement_length,
    bool replacement_is_null,
    struct session_scalar_cell *out_cell
) {
    bool any_argument_is_null = false;

    if (value_is_null || pattern_is_null || replacement_is_null) {
        any_argument_is_null = true;
    }

    if (any_argument_is_null) {
        out_cell->value = NULL;
        return MYLITE_OK;
    }
    if (pattern_length == 0U) {
        mylite_execution_set_regexp_illegal_argument_error(database);
        return MYLITE_ERROR;
    }
    if (kind == PLANNED_REGEXP_STRING_FUNCTION_INSTR ||
        kind == PLANNED_REGEXP_STRING_FUNCTION_SUBSTR) {
        return regexp_instr_or_substr_result_value(
            database,
            kind,
            value,
            value_length,
            pattern,
            pattern_length,
            out_cell
        );
    }
    if (kind == PLANNED_REGEXP_STRING_FUNCTION_REPLACE) {
        return regexp_replace_result_value(
            database,
            value,
            value_length,
            pattern,
            pattern_length,
            replacement,
            replacement_length,
            out_cell
        );
    }
    return MYLITE_MISUSE;
}

static int regexp_instr_or_substr_result_value(
    struct mylite_db *database,
    enum planned_regexp_string_function_kind kind,
    const char *value,
    size_t value_length,
    const char *pattern,
    size_t pattern_length,
    struct session_scalar_cell *out_cell
) {
    struct mylite_regexp_match match = {
        .matched = false,
        .start = 0U,
        .end = 0U,
    };
    int rc = MYLITE_OK;

    rc = regexp_string_find_match(database, value, value_length, pattern, pattern_length, &match);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (kind == PLANNED_REGEXP_STRING_FUNCTION_INSTR) {
        uint64_t position = 0U;

        if (match.matched) {
            position = (uint64_t)match.start + 1U;
        }
        return mylite_execution_format_session_scalar_uint64_value(database, position, out_cell);
    }
    if (!match.matched) {
        out_cell->value = NULL;
        return MYLITE_OK;
    }
    return regexp_substr_result_value(database, value, &match, out_cell);
}

static int regexp_substr_result_value(
    struct mylite_db *database,
    const char *value,
    const struct mylite_regexp_match *match,
    struct session_scalar_cell *out_cell
) {
    size_t length = 0U;

    if (value == NULL || match == NULL || out_cell == NULL || match->end < match->start) {
        return MYLITE_MISUSE;
    }
    length = match->end - match->start;
    out_cell->owned_text = (char *)malloc(length + 1U);
    if (out_cell->owned_text == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (length != 0U) {
        memcpy(out_cell->owned_text, value + match->start, length);
    }
    out_cell->owned_text[length] = '\0';
    out_cell->value = out_cell->owned_text;
    out_cell->value_size = length;
    out_cell->has_value_size = true;
    return MYLITE_OK;
}

static int regexp_replace_result_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    const char *pattern,
    size_t pattern_length,
    const char *replacement,
    size_t replacement_length,
    struct session_scalar_cell *out_cell
) {
    struct mylite_regexp_program *program = NULL;
    enum mylite_regexp_compile_status compile_status = MYLITE_REGEXP_COMPILE_OK;
    enum mylite_regexp_match_status match_status = MYLITE_REGEXP_MATCH_OK;
    size_t append_offset = 0U;
    size_t search_offset = 0U;
    int rc = MYLITE_OK;

    compile_status = mylite_regexp_compile_ascii_ci(pattern, pattern_length, &program);
    if (compile_status != MYLITE_REGEXP_COMPILE_OK) {
        mylite_regexp_program_free(program);
        return regexp_string_compile_error(database, compile_status);
    }
    if (value_length == 0U) {
        mylite_regexp_program_free(program);
        return regexp_replace_append(database, out_cell, "", 0U);
    }

    while (search_offset <= value_length) {
        struct mylite_regexp_match match = {
            .matched = false,
            .start = 0U,
            .end = 0U,
        };

        match_status = mylite_regexp_program_find_ascii_ci(
            program,
            value,
            value_length,
            search_offset,
            &match
        );
        if (match_status != MYLITE_REGEXP_MATCH_OK) {
            mylite_regexp_program_free(program);
            return regexp_string_match_error(database, match_status);
        }
        if (!match.matched) {
            break;
        }
        rc = regexp_replace_append(
            database,
            out_cell,
            value + append_offset,
            match.start - append_offset
        );
        if (rc == MYLITE_OK) {
            rc = regexp_replace_append(database, out_cell, replacement, replacement_length);
        }
        if (rc != MYLITE_OK) {
            mylite_regexp_program_free(program);
            return rc;
        }

        append_offset = match.end;
        search_offset = match.end;
        if (match.start == match.end) {
            if (search_offset >= value_length) {
                break;
            }
            rc = regexp_replace_append(database, out_cell, value + search_offset, 1U);
            if (rc != MYLITE_OK) {
                mylite_regexp_program_free(program);
                return rc;
            }
            ++search_offset;
            append_offset = search_offset;
        }
    }

    mylite_regexp_program_free(program);
    rc = regexp_replace_append(
        database,
        out_cell,
        value + append_offset,
        value_length - append_offset
    );
    if (rc == MYLITE_OK && out_cell->owned_text == NULL) {
        rc = regexp_replace_append(database, out_cell, "", 0U);
    }
    return rc;
}

static int regexp_replace_append(
    struct mylite_db *database,
    struct session_scalar_cell *out_cell,
    const char *text,
    size_t text_length
) {
    char *buffer = NULL;
    size_t current_length = 0U;

    if (out_cell == NULL || (text == NULL && text_length != 0U)) {
        return MYLITE_MISUSE;
    }
    if (out_cell->has_value_size) {
        current_length = out_cell->value_size;
    }
    if (current_length > SIZE_MAX - text_length - 1U) {
        mylite_execution_set_unsupported_error(database, "REGEXP_REPLACE() result is too large");
        return MYLITE_ERROR;
    }
    buffer = (char *)realloc(out_cell->owned_text, current_length + text_length + 1U);
    if (buffer == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_cell->owned_text = buffer;
    if (text_length != 0U) {
        memcpy(out_cell->owned_text + current_length, text, text_length);
    }
    out_cell->value_size = current_length + text_length;
    out_cell->owned_text[out_cell->value_size] = '\0';
    out_cell->has_value_size = true;
    out_cell->value = out_cell->owned_text;
    return MYLITE_OK;
}
