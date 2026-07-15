#include "mylite_execution_scalar_temporal_format.h"
#include "mylite_execution_scalar.h"

#include "mylite_ast.h"
#include "mylite_catalog.h"
#include "mylite_date_format.h"
#include "mylite_date_interval_second.h"
#include "mylite_diagnostics.h"
#include "mylite_temporal_arithmetic.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    scalar_temporal_node_stack_initial_capacity = 16,
};

struct scalar_temporal_node_stack {
    const struct mylite_sql_ast_node **items;
    size_t count;
    size_t capacity;
};

static int date_format_function_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_value,
    size_t *out_value_length,
    bool *out_value_is_null,
    char **out_format,
    size_t *out_format_length,
    bool *out_format_is_null
);
static int time_format_function_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_value,
    size_t *out_value_length,
    bool *out_value_is_null,
    char **out_format,
    size_t *out_format_length,
    bool *out_format_is_null
);
static int str_to_date_function_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_value,
    size_t *out_value_length,
    bool *out_value_is_null,
    char **out_format,
    size_t *out_format_length,
    bool *out_format_is_null
);
static int str_to_date_set_unknown_identifier_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static bool scalar_temporal_node_stack_push_children_reverse(
    struct scalar_temporal_node_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static bool scalar_temporal_node_stack_push(
    struct scalar_temporal_node_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static void scalar_temporal_node_stack_deinit(struct scalar_temporal_node_stack *stack);
static int date_format_string_or_null_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_message,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int get_format_string_or_null_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_message,
    const char *literal_nul_message,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int get_format_literal_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int get_format_copy_mapped_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *class_node,
    const char *format_text,
    size_t format_text_length,
    struct session_scalar_cell *out_cell
);
static const char *get_format_lookup(
    const struct mylite_sql_source_span *class_span,
    const char *format_text,
    size_t format_text_length
);
static size_t get_format_class_index(const struct mylite_sql_source_span *class_span);
static size_t get_format_name_index(const char *format_text, size_t format_text_length);
static bool span_text_equals_ascii_case_insensitive(
    const struct mylite_sql_source_span *span,
    const char *text
);
static bool bytes_text_equals_ascii_case_insensitive(
    const char *left,
    size_t left_length,
    const char *right
);
static int time_format_string_or_null_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_message,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int str_to_date_string_or_null_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_message,
    const char *literal_nul_message,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int date_format_numeric_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    double *out_value
);
static bool date_format_numeric_literal_expression(const struct mylite_sql_ast_node *expression);
static bool date_format_numeric_comparison_operator_is_supported(
    enum mylite_sql_ast_operator operator_kind
);
static enum mylite_sql_ast_operator date_format_numeric_comparison_invert_operator(
    enum mylite_sql_ast_operator operator_kind
);
static bool date_format_numeric_comparison_truth(
    double left,
    double right,
    enum mylite_sql_ast_operator operator_kind
);

int mylite_execution_scalar_date_format_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    char *value = NULL;
    char *format = NULL;
    size_t value_length = 0U;
    size_t format_length = 0U;
    bool value_is_null = false;
    bool format_is_null = false;
    bool result_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    rc = date_format_function_arguments(
        database,
        expression,
        &value,
        &value_length,
        &value_is_null,
        &format,
        &format_length,
        &format_is_null
    );
    if (rc == MYLITE_OK && !value_is_null && !format_is_null) {
        rc = mylite_date_format_value(
            database,
            value,
            value_length,
            MYLITE_DATE_FORMAT_INPUT_STRING,
            format,
            format_length,
            &out_cell->owned_text,
            &result_is_null
        );
    } else {
        result_is_null = true;
    }
    if (rc == MYLITE_OK && !result_is_null) {
        out_cell->value = out_cell->owned_text;
    }
    free(value);
    free(format);
    return rc;
}

int mylite_execution_scalar_get_format_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *class_node = NULL;
    char *format_text = NULL;
    size_t format_text_length = 0U;
    bool format_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_GET_FORMAT_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        mylite_execution_set_unsupported_error(
            database,
            "GET_FORMAT() supports only literal format names"
        );
        return MYLITE_ERROR;
    }

    class_node = mylite_execution_child_at(expression, 0U);
    rc = get_format_literal_argument(
        database,
        mylite_execution_child_at(expression, 1U),
        &format_text,
        &format_text_length,
        &format_is_null
    );
    if (rc == MYLITE_OK && !format_is_null) {
        rc = get_format_copy_mapped_value(
            database,
            class_node,
            format_text,
            format_text_length,
            out_cell
        );
    }
    free(format_text);
    return rc;
}

int mylite_execution_scalar_time_format_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    char *value = NULL;
    char *format = NULL;
    size_t value_length = 0U;
    size_t format_length = 0U;
    bool value_is_null = false;
    bool format_is_null = false;
    bool result_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    rc = time_format_function_arguments(
        database,
        expression,
        &value,
        &value_length,
        &value_is_null,
        &format,
        &format_length,
        &format_is_null
    );
    if (rc == MYLITE_OK && !value_is_null && !format_is_null) {
        rc = mylite_time_format_value(
            database,
            value,
            value_length,
            MYLITE_DATE_FORMAT_INPUT_STRING,
            format,
            format_length,
            &out_cell->owned_text,
            &result_is_null
        );
    } else {
        result_is_null = true;
    }
    if (rc == MYLITE_OK && !result_is_null) {
        out_cell->value = out_cell->owned_text;
    }
    free(value);
    free(format);
    return rc;
}

int mylite_execution_scalar_str_to_date_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    char *value = NULL;
    char *format = NULL;
    size_t value_length = 0U;
    size_t format_length = 0U;
    bool value_is_null = false;
    bool format_is_null = false;
    bool result_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    rc = str_to_date_function_arguments(
        database,
        expression,
        &value,
        &value_length,
        &value_is_null,
        &format,
        &format_length,
        &format_is_null
    );
    if (rc == MYLITE_OK && !value_is_null && !format_is_null) {
        rc = mylite_str_to_date_value(
            database,
            value,
            value_length,
            format,
            format_length,
            &out_cell->owned_text,
            &result_is_null
        );
    } else {
        result_is_null = true;
    }
    if (rc == MYLITE_OK && !result_is_null) {
        out_cell->value = out_cell->owned_text;
    }
    free(value);
    free(format);
    return rc;
}

static int date_format_function_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_value,
    size_t *out_value_length,
    bool *out_value_is_null,
    char **out_format,
    size_t *out_format_length,
    bool *out_format_is_null
) {
    int rc = MYLITE_OK;

    if (out_value == NULL || out_value_length == NULL || out_value_is_null == NULL ||
        out_format == NULL || out_format_length == NULL || out_format_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = NULL;
    *out_value_length = 0U;
    *out_value_is_null = false;
    *out_format = NULL;
    *out_format_length = 0U;
    *out_format_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_DATE_FORMAT_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        mylite_execution_set_native_function_parameter_count_error(database, "DATE_FORMAT");
        return MYLITE_ERROR;
    }

    rc = date_format_string_or_null_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        "DATE_FORMAT() supports only string, DATE, DATETIME, TIMESTAMP, and NULL values",
        out_value,
        out_value_length,
        out_value_is_null
    );
    if (rc == MYLITE_OK) {
        rc = get_format_string_or_null_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            "DATE_FORMAT() supports only string format literals and NULL",
            "DATE_FORMAT() literals do not support NUL bytes",
            out_format,
            out_format_length,
            out_format_is_null
        );
    }
    if (rc == MYLITE_OK && !*out_format_is_null) {
        rc = mylite_date_format_validate_format(database, *out_format, *out_format_length);
    }
    return rc;
}

static int time_format_function_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_value,
    size_t *out_value_length,
    bool *out_value_is_null,
    char **out_format,
    size_t *out_format_length,
    bool *out_format_is_null
) {
    int rc = MYLITE_OK;

    if (out_value == NULL || out_value_length == NULL || out_value_is_null == NULL ||
        out_format == NULL || out_format_length == NULL || out_format_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = NULL;
    *out_value_length = 0U;
    *out_value_is_null = false;
    *out_format = NULL;
    *out_format_length = 0U;
    *out_format_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_TIME_FORMAT_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        mylite_execution_set_native_function_parameter_count_error(database, "TIME_FORMAT");
        return MYLITE_ERROR;
    }

    rc = time_format_string_or_null_argument(
        database,
        mylite_execution_child_at(expression, 0U),
        "TIME_FORMAT() supports only string, DATE, TIME, DATETIME, TIMESTAMP, and NULL values",
        out_value,
        out_value_length,
        out_value_is_null
    );
    if (rc == MYLITE_OK) {
        rc = get_format_string_or_null_argument(
            database,
            mylite_execution_child_at(expression, 1U),
            "TIME_FORMAT() supports only string format literals and NULL",
            "TIME_FORMAT() literals do not support NUL bytes",
            out_format,
            out_format_length,
            out_format_is_null
        );
    }
    if (rc == MYLITE_OK && !*out_format_is_null) {
        rc = mylite_time_format_validate_format(database, *out_format, *out_format_length);
    }
    return rc;
}

static int str_to_date_function_arguments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_value,
    size_t *out_value_length,
    bool *out_value_is_null,
    char **out_format,
    size_t *out_format_length,
    bool *out_format_is_null
) {
    const struct mylite_sql_ast_node *value_expression = NULL;
    const struct mylite_sql_ast_node *format_expression = NULL;
    int rc = MYLITE_OK;

    if (out_value == NULL || out_value_length == NULL || out_value_is_null == NULL ||
        out_format == NULL || out_format_length == NULL || out_format_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = NULL;
    *out_value_length = 0U;
    *out_value_is_null = false;
    *out_format = NULL;
    *out_format_length = 0U;
    *out_format_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_STR_TO_DATE_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        mylite_execution_set_native_function_parameter_count_error(database, "STR_TO_DATE");
        return MYLITE_ERROR;
    }

    value_expression = mylite_execution_child_at(expression, 0U);
    format_expression = mylite_execution_child_at(expression, 1U);
    if (mylite_execution_str_to_date_child_is_null_literal(value_expression)) {
        rc = str_to_date_set_unknown_identifier_reference(database, format_expression);
        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_value_is_null = true;
        return MYLITE_OK;
    }
    if (mylite_execution_str_to_date_child_is_null_literal(format_expression)) {
        rc = str_to_date_set_unknown_identifier_reference(database, value_expression);
        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_format_is_null = true;
        return MYLITE_OK;
    }

    rc = str_to_date_string_or_null_argument(
        database,
        value_expression,
        "STR_TO_DATE() supports only string and NULL values",
        "STR_TO_DATE() literals do not support NUL bytes",
        out_value,
        out_value_length,
        out_value_is_null
    );
    if (rc == MYLITE_OK) {
        rc = get_format_string_or_null_argument(
            database,
            format_expression,
            "STR_TO_DATE() supports only string format literals and NULL",
            "STR_TO_DATE() literals do not support NUL bytes",
            out_format,
            out_format_length,
            out_format_is_null
        );
    }
    if (rc == MYLITE_OK && !*out_format_is_null) {
        rc = mylite_str_to_date_validate_format(database, *out_format, *out_format_length);
    }
    return rc;
}

bool mylite_execution_str_to_date_child_is_null_literal(const struct mylite_sql_ast_node *expression
) {
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    return (expression != NULL && expression->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) != 0;
}

bool mylite_execution_str_to_date_child_is_identifier_reference(
    const struct mylite_sql_ast_node *expression
) {
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    return (expression != NULL && (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
                                   expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) != 0;
}

static int str_to_date_set_unknown_identifier_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    struct scalar_temporal_node_stack stack = {0};

    if (expression == NULL) {
        return MYLITE_OK;
    }
    if (!scalar_temporal_node_stack_push(&stack, expression)) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    while (stack.count != 0U) {
        const struct mylite_sql_ast_node *current =
            mylite_execution_unwrap_parenthesized_expression(stack.items[--stack.count]);

        if (current == NULL) {
            continue;
        }
        if (mylite_execution_str_to_date_child_is_identifier_reference(current)) {
            scalar_temporal_node_stack_deinit(&stack);
            return mylite_execution_date_add_set_unknown_identifier_error(database, current);
        }

        if (!scalar_temporal_node_stack_push_children_reverse(&stack, current)) {
            scalar_temporal_node_stack_deinit(&stack);
            mylite_execution_set_nomem_error(database);
            return MYLITE_NOMEM;
        }
    }
    scalar_temporal_node_stack_deinit(&stack);
    return MYLITE_OK;
}

static int date_format_string_or_null_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_message,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return mylite_execution_date_add_set_unknown_identifier_error(database, expression);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
        mylite_execution_set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    rc = mylite_execution_decode_sql_string_literal(
        database,
        expression,
        unsupported_message,
        "DATE_FORMAT() literals do not support NUL bytes",
        out_text,
        out_text_length
    );
    if (rc == MYLITE_OK && memchr(*out_text, '\0', *out_text_length) != NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "DATE_FORMAT() literals do not support NUL bytes"
        );
        rc = MYLITE_ERROR;
    }
    return rc;
}

static int get_format_string_or_null_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_message,
    const char *literal_nul_message,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    struct session_scalar_cell cell = {0};
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    if (expression->kind != MYLITE_SQL_AST_GET_FORMAT_FUNCTION) {
        return str_to_date_string_or_null_argument(
            database,
            expression,
            unsupported_message,
            literal_nul_message,
            out_text,
            out_text_length,
            out_is_null
        );
    }

    rc = mylite_execution_scalar_get_format_function_value(database, expression, &cell);
    if (rc == MYLITE_OK && cell.value == NULL) {
        *out_is_null = true;
    } else if (rc == MYLITE_OK) {
        *out_text_length = strlen(cell.value);
        if (memchr(cell.value, '\0', *out_text_length) != NULL) {
            mylite_execution_set_unsupported_error(database, literal_nul_message);
            rc = MYLITE_ERROR;
        } else {
            *out_text = cell.owned_text;
            cell.owned_text = NULL;
        }
    }
    mylite_execution_session_scalar_cell_deinit(&cell);
    return rc;
}

static int get_format_literal_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    const struct mylite_sql_ast_node *literal = NULL;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (out_text == NULL || out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    literal = mylite_execution_unwrap_parenthesized_expression(expression);
    if (literal == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "GET_FORMAT() supports only literal format names"
        );
        return MYLITE_ERROR;
    }
    if (literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(literal);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            mylite_execution_set_unsupported_error(
                database,
                "GET_FORMAT() supports only literal format names"
            );
            return MYLITE_ERROR;
        }
        literal =
            mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(literal, 0U)
            );
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            mylite_execution_set_unsupported_error(
                database,
                "GET_FORMAT() supports only literal format names"
            );
            return MYLITE_ERROR;
        }
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal->kind != MYLITE_SQL_AST_LITERAL) {
        mylite_execution_set_unsupported_error(
            database,
            "GET_FORMAT() supports only literal format names"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL ||
        literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
        literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        mylite_execution_set_unsupported_error(
            database,
            "GET_FORMAT() supports only literal format names"
        );
        return MYLITE_ERROR;
    }

    if (mylite_execution_decode_sql_string_literal(
            database,
            literal,
            "GET_FORMAT() supports only literal format names",
            "GET_FORMAT() format names do not support NUL bytes",
            out_text,
            out_text_length
        ) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    if (memchr(*out_text, '\0', *out_text_length) != NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "GET_FORMAT() format names do not support NUL bytes"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int get_format_copy_mapped_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *class_node,
    const char *format_text,
    size_t format_text_length,
    struct session_scalar_cell *out_cell
) {
    const char *mapped = NULL;
    size_t mapped_length = 0U;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    class_node = mylite_execution_unwrap_parenthesized_expression(class_node);
    if (class_node == NULL || class_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
        mylite_execution_set_unsupported_error(
            database,
            "GET_FORMAT() supports only DATE, TIME, DATETIME, and TIMESTAMP classes"
        );
        return MYLITE_ERROR;
    }

    mapped = get_format_lookup(&class_node->span, format_text, format_text_length);
    if (mapped == NULL) {
        return MYLITE_OK;
    }

    mapped_length = strlen(mapped);
    out_cell->owned_text = (char *)malloc(mapped_length + 1U);
    if (out_cell->owned_text == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(out_cell->owned_text, mapped, mapped_length + 1U);
    out_cell->value = out_cell->owned_text;
    return MYLITE_OK;
}

static const char *get_format_lookup(
    const struct mylite_sql_source_span *class_span,
    const char *format_text,
    size_t format_text_length
) {
    static const char *const values[3U][5U] = {
        {"%m.%d.%Y", "%Y-%m-%d", "%Y-%m-%d", "%d.%m.%Y", "%Y%m%d"},
        {"%h:%i:%s %p", "%H:%i:%s", "%H:%i:%s", "%H.%i.%s", "%H%i%s"},
        {
            "%Y-%m-%d %H.%i.%s",
            "%Y-%m-%d %H:%i:%s",
            "%Y-%m-%d %H:%i:%s",
            "%Y-%m-%d %H.%i.%s",
            "%Y%m%d%H%i%s",
        },
    };
    size_t class_index = get_format_class_index(class_span);
    size_t name_index = get_format_name_index(format_text, format_text_length);

    if (class_index == SIZE_MAX || name_index == SIZE_MAX) {
        return NULL;
    }
    return values[class_index][name_index];
}

static size_t get_format_class_index(const struct mylite_sql_source_span *class_span) {
    if (span_text_equals_ascii_case_insensitive(class_span, "DATE")) {
        return 0U;
    }
    if (span_text_equals_ascii_case_insensitive(class_span, "TIME")) {
        return 1U;
    }
    if (span_text_equals_ascii_case_insensitive(class_span, "DATETIME") ||
        span_text_equals_ascii_case_insensitive(class_span, "TIMESTAMP")) {
        return 2U;
    }
    return SIZE_MAX;
}

static size_t get_format_name_index(const char *format_text, size_t format_text_length) {
    static const char *const names[] = {"USA", "JIS", "ISO", "EUR", "INTERNAL"};

    for (size_t index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        if (bytes_text_equals_ascii_case_insensitive(
                format_text,
                format_text_length,
                names[index]
            )) {
            return index;
        }
    }
    return SIZE_MAX;
}

static bool span_text_equals_ascii_case_insensitive(
    const struct mylite_sql_source_span *span,
    const char *text
) {
    if (span == NULL) {
        return false;
    }
    return bytes_text_equals_ascii_case_insensitive(span->text, span->length, text);
}

static bool bytes_text_equals_ascii_case_insensitive(
    const char *left,
    size_t left_length,
    const char *right
) {
    size_t right_length = right == NULL ? 0U : strlen(right);

    if (left == NULL || right == NULL || left_length != right_length) {
        return false;
    }
    for (size_t index = 0U; index < left_length; ++index) {
        if (toupper((unsigned char)left[index]) != toupper((unsigned char)right[index])) {
            return false;
        }
    }
    return true;
}

static int time_format_string_or_null_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_message,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return mylite_execution_date_add_set_unknown_identifier_error(database, expression);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
        mylite_execution_set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    rc = mylite_execution_decode_sql_string_literal(
        database,
        expression,
        unsupported_message,
        "TIME_FORMAT() literals do not support NUL bytes",
        out_text,
        out_text_length
    );
    if (rc == MYLITE_OK && memchr(*out_text, '\0', *out_text_length) != NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "TIME_FORMAT() literals do not support NUL bytes"
        );
        rc = MYLITE_ERROR;
    }
    return rc;
}

static int str_to_date_string_or_null_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_message,
    const char *literal_nul_message,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return mylite_execution_date_add_set_unknown_identifier_error(database, expression);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
        mylite_execution_set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    rc = mylite_execution_decode_sql_string_literal(
        database,
        expression,
        unsupported_message,
        literal_nul_message,
        out_text,
        out_text_length
    );
    if (rc == MYLITE_OK && memchr(*out_text, '\0', *out_text_length) != NULL) {
        mylite_execution_set_unsupported_error(database, literal_nul_message);
        rc = MYLITE_ERROR;
    }
    return rc;
}

int mylite_execution_scalar_date_format_numeric_comparison_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *date_format = NULL;
    const struct mylite_sql_ast_node *numeric = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    char *value = NULL;
    char *format = NULL;
    char *formatted = NULL;
    size_t value_length = 0U;
    size_t format_length = 0U;
    bool value_is_null = false;
    bool format_is_null = false;
    bool result_is_null = false;
    double left = 0.0;
    double right = 0.0;
    char *end = NULL;
    int rc = MYLITE_OK;
    int written = 0;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (!mylite_execution_date_format_numeric_comparison_sides(
            expression,
            &date_format,
            &numeric,
            &operator_kind
        )) {
        mylite_execution_set_unsupported_error(
            database,
            "DATE_FORMAT() numeric comparison supports only DATE_FORMAT(value, format) "
            "[=, <>, <, <=, >, >=] numeric_literal"
        );
        return MYLITE_ERROR;
    }

    rc = date_format_function_arguments(
        database,
        date_format,
        &value,
        &value_length,
        &value_is_null,
        &format,
        &format_length,
        &format_is_null
    );
    if (rc == MYLITE_OK && !format_is_null &&
        !mylite_execution_date_format_numeric_comparison_format_is_supported(
            format,
            format_length
        )) {
        mylite_execution_set_unsupported_error(
            database,
            "DATE_FORMAT() numeric comparison supports only DATE_FORMAT(value, '%H.%i' or "
            "'%H.%i%s') "
            "[=, <>, <, <=, >, >=] numeric_literal"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && !value_is_null && !format_is_null) {
        rc = mylite_date_format_value(
            database,
            value,
            value_length,
            MYLITE_DATE_FORMAT_INPUT_STRING,
            format,
            format_length,
            &formatted,
            &result_is_null
        );
    } else {
        result_is_null = true;
    }
    if (rc == MYLITE_OK) {
        rc = date_format_numeric_literal_value(database, numeric, &right);
    }
    if (rc != MYLITE_OK) {
        free(value);
        free(format);
        free(formatted);
        return rc;
    }
    if (result_is_null) {
        free(value);
        free(format);
        free(formatted);
        return MYLITE_OK;
    }

    left = strtod(formatted, &end);
    if (end == formatted || *end != '\0') {
        mylite_execution_set_unsupported_error(
            database,
            "DATE_FORMAT() numeric comparison supports only numeric DATE_FORMAT() output"
        );
        free(value);
        free(format);
        free(formatted);
        return MYLITE_ERROR;
    }
    written = snprintf(
        out_cell->integer_text,
        sizeof(out_cell->integer_text),
        "%d",
        date_format_numeric_comparison_truth(left, right, operator_kind) ? 1 : 0
    );
    if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
        mylite_execution_set_runtime_error(
            database,
            "failed to format DATE_FORMAT() comparison value"
        );
        free(value);
        free(format);
        free(formatted);
        return MYLITE_ERROR;
    }
    out_cell->value = out_cell->integer_text;
    free(value);
    free(format);
    free(formatted);
    return MYLITE_OK;
}

bool mylite_execution_scalar_is_date_format_numeric_comparison_expression(
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node *date_format = NULL;
    const struct mylite_sql_ast_node *numeric = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;

    return mylite_execution_date_format_numeric_comparison_sides(
        expression,
        &date_format,
        &numeric,
        &operator_kind
    );
}

bool mylite_execution_scalar_is_date_format_comparison_expression(
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node *left = NULL;
    const struct mylite_sql_ast_node *right = NULL;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return false;
    }
    switch (mylite_sql_ast_node_operator(expression)) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        break;
    default:
        return false;
    }
    left =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 0U));
    right =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 1U));
    return ((left != NULL && left->kind == MYLITE_SQL_AST_DATE_FORMAT_FUNCTION) ||
            (right != NULL && right->kind == MYLITE_SQL_AST_DATE_FORMAT_FUNCTION)) != 0;
}

bool mylite_execution_date_format_numeric_comparison_sides(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node **out_date_format,
    const struct mylite_sql_ast_node **out_numeric,
    enum mylite_sql_ast_operator *out_operator_kind
) {
    const struct mylite_sql_ast_node *left = NULL;
    const struct mylite_sql_ast_node *right = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (out_date_format == NULL || out_numeric == NULL || out_operator_kind == NULL ||
        expression == NULL || expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return false;
    }
    operator_kind = mylite_sql_ast_node_operator(expression);
    if (!date_format_numeric_comparison_operator_is_supported(operator_kind)) {
        return false;
    }
    left =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 0U));
    right =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 1U));
    if (left != NULL && left->kind == MYLITE_SQL_AST_DATE_FORMAT_FUNCTION &&
        date_format_numeric_literal_expression(right)) {
        *out_date_format = left;
        *out_numeric = right;
        *out_operator_kind = operator_kind;
        return true;
    }
    if (right != NULL && right->kind == MYLITE_SQL_AST_DATE_FORMAT_FUNCTION &&
        date_format_numeric_literal_expression(left)) {
        *out_date_format = right;
        *out_numeric = left;
        *out_operator_kind = date_format_numeric_comparison_invert_operator(operator_kind);
        return true;
    }
    return false;
}

static bool date_format_numeric_literal_expression(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *literal =
        mylite_execution_unwrap_parenthesized_expression(expression);

    if (literal != NULL && literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(literal);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        literal =
            mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(literal, 0U)
            );
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }
    return (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER ||
            mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_DECIMAL) != 0;
}

static bool date_format_numeric_comparison_operator_is_supported(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return true;
    default:
        return false;
    }
}

static enum mylite_sql_ast_operator date_format_numeric_comparison_invert_operator(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return MYLITE_SQL_AST_OPERATOR_GREATER;
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL;
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return MYLITE_SQL_AST_OPERATOR_LESS;
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return MYLITE_SQL_AST_OPERATOR_LESS_EQUAL;
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return operator_kind;
    default:
        return MYLITE_SQL_AST_OPERATOR_NONE;
    }
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters): SQL comparison order is left/right.
static bool date_format_numeric_comparison_truth(
    double left,
    double right,
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
        return left == right;
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return left != right;
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return left < right;
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return left <= right;
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return left > right;
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return left >= right;
    default:
        return false;
    }
}

// NOLINTEND(bugprone-easily-swappable-parameters)

static int date_format_numeric_literal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    double *out_value
) {
    const struct mylite_sql_ast_node *literal = expression;
    bool is_negative = false;
    size_t text_size = 0U;
    char *text = NULL;
    char *end = NULL;
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0.0;
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            mylite_execution_set_unsupported_error(
                database,
                "DATE_FORMAT() numeric comparison requires a literal"
            );
            return MYLITE_ERROR;
        }
        literal = mylite_execution_unwrap_parenthesized_expression(
            mylite_execution_child_at(expression, 0U)
        );
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        (mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER &&
         mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_DECIMAL)) {
        mylite_execution_set_unsupported_error(
            database,
            "DATE_FORMAT() numeric comparison requires a literal"
        );
        return MYLITE_ERROR;
    }
    text_size = literal->span.length + 1U;
    if (is_negative) {
        ++text_size;
    }
    text = (char *)malloc(text_size);
    if (text == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (is_negative) {
        text[0] = '-';
        memcpy(text + 1, literal->span.text, literal->span.length);
        text[literal->span.length + 1U] = '\0';
    } else {
        memcpy(text, literal->span.text, literal->span.length);
        text[literal->span.length] = '\0';
    }
    *out_value = strtod(text, &end);
    if (end == text || *end != '\0') {
        mylite_execution_set_unsupported_error(
            database,
            "DATE_FORMAT() numeric comparison requires a literal"
        );
        rc = MYLITE_ERROR;
    }
    free(text);
    return rc;
}

bool mylite_execution_date_format_numeric_comparison_format_is_supported(
    const char *format,
    size_t format_length
) {
    static const char supported_formats[][8] = {"%H.%i", "%H.%i%s"};

    if (format == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(supported_formats) / sizeof(supported_formats[0]);
         ++index) {
        size_t supported_length = strlen(supported_formats[index]);

        if (format_length == supported_length &&
            memcmp(format, supported_formats[index], supported_length) == 0) {
            return true;
        }
    }
    return false;
}

static bool scalar_temporal_node_stack_push_children_reverse(
    struct scalar_temporal_node_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node *child = expression == NULL ? NULL : expression->first_child;
    size_t first_child_index = 0U;
    size_t left = 0U;
    size_t right = 0U;

    if (stack == NULL) {
        return false;
    }
    first_child_index = stack->count;
    while (child != NULL) {
        if (!scalar_temporal_node_stack_push(stack, child)) {
            return false;
        }
        child = child->next_sibling;
    }
    left = first_child_index;
    right = stack->count;
    while (left < right) {
        const struct mylite_sql_ast_node *item = stack->items[left];

        --right;
        if (left >= right) {
            break;
        }
        stack->items[left] = stack->items[right];
        stack->items[right] = item;
        ++left;
    }
    return true;
}

static bool scalar_temporal_node_stack_push(
    struct scalar_temporal_node_stack *stack,
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node **items = NULL;
    size_t capacity = 0U;

    if (stack == NULL) {
        return false;
    }
    if (stack->count == stack->capacity) {
        capacity = stack->capacity == 0U ? scalar_temporal_node_stack_initial_capacity
                                         : stack->capacity * 2U;
        if (capacity < stack->capacity || capacity > SIZE_MAX / sizeof(*items)) {
            return false;
        }
        items = (const struct mylite_sql_ast_node **)
            realloc((void *)stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            return false;
        }
        stack->items = items;
        stack->capacity = capacity;
    }

    stack->items[stack->count] = expression;
    ++stack->count;
    return true;
}

static void scalar_temporal_node_stack_deinit(struct scalar_temporal_node_stack *stack) {
    if (stack == NULL) {
        return;
    }

    free((void *)stack->items);
    *stack = (struct scalar_temporal_node_stack){0};
}
