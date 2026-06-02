#include "mylite_execution_scalar_temporal_format.h"
#include "mylite_execution_scalar.h"

#include "mylite_ast.h"
#include "mylite_catalog.h"
#include "mylite_date_format.h"
#include "mylite_date_interval_second.h"
#include "mylite_diagnostics.h"
#include "mylite_temporal_arithmetic.h"

#include <mylite/mylite.h>

#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    date_text_length = 10,
    datetime_text_length = mylite_execution_scalar_datetime_text_length,
    decimal_base = 10,
    date_minimum_year = 1000,
    date_maximum_year = 9999,
    date_months_per_year = 12,
    date_interval_diagnostic_capacity = 160,
    date_interval_nul_diagnostic_capacity = 96,
    date_interval_format_diagnostic_capacity = 96,
    date_interval_days_per_week = 7,
    date_interval_months_per_quarter = 3,
    time_text_minimum_length = 8,
    time_text_maximum_length = 10,
    time_minute_second_suffix_length = 6,
    time_minimum_three_digit_hour = 100,
    time_maximum_hour = 838,
    time_maximum_minute_or_second = 59,
    time_second_per_minute = 60,
    time_second_per_hour = 3600,
    scalar_temporal_node_stack_initial_capacity = 16,
};

enum scalar_time_arithmetic_input_kind {
    SCALAR_TIME_ARITHMETIC_INPUT_NULL = 0,
    SCALAR_TIME_ARITHMETIC_INPUT_TIME = 1,
    SCALAR_TIME_ARITHMETIC_INPUT_DATETIME = 2,
};

struct scalar_time_arithmetic_input {
    enum scalar_time_arithmetic_input_kind kind;
    int64_t time_seconds;
    struct mylite_temporal_datetime_parts datetime;
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
static int date_interval_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int timestampadd_second_unit_from_ast(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *unit
);
static int set_date_interval_second_unsupported_error(
    struct mylite_db *database,
    const char *function_name,
    const char *suffix
);
static int set_date_interval_second_unsupported_shape_error(
    struct mylite_db *database,
    const char *function_name
);
static int date_interval_second_temporal_argument(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *expression,
    struct mylite_temporal_datetime_parts *out_datetime,
    bool *out_has_time,
    bool *out_is_null
);
static const char *date_interval_literal_support_text(const char *function_name);
static int set_date_interval_argument_support_error(
    struct mylite_db *database,
    const char *function_name,
    enum mylite_date_interval_unit unit
);
static int set_date_interval_argument_range_error(
    struct mylite_db *database,
    const char *function_name,
    enum mylite_date_interval_unit unit
);
static int date_interval_apply(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_temporal_datetime_parts *input,
    int64_t interval_value,
    enum mylite_date_interval_unit unit,
    struct mylite_temporal_datetime_parts *out_datetime
);
static int date_interval_apply_calendar_months(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_temporal_datetime_parts *input,
    int64_t interval_months,
    struct mylite_temporal_datetime_parts *out_datetime
);
static int date_interval_second_apply(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_temporal_datetime_parts *input,
    int64_t interval_seconds,
    struct mylite_temporal_datetime_parts *out_datetime
);
static int date_interval_second_format(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_temporal_datetime_parts *datetime,
    struct session_scalar_cell *out_cell
);
static int date_interval_format(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_temporal_datetime_parts *datetime,
    bool result_has_time,
    struct session_scalar_cell *out_cell
);
static const char *time_arithmetic_function_name(enum mylite_sql_ast_node_kind kind);
static bool time_arithmetic_function_subtracts(enum mylite_sql_ast_node_kind kind);
static int set_time_arithmetic_unsupported_error(
    struct mylite_db *database,
    const char *function_name,
    const char *suffix
);
static bool time_arithmetic_message(
    char *buffer,
    size_t buffer_size,
    const char *function_name,
    const char *suffix
);
static int time_arithmetic_first_argument(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *expression,
    struct scalar_time_arithmetic_input *out_input
);
static int time_arithmetic_second_argument(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *expression,
    struct scalar_time_arithmetic_input *out_input
);
static int time_arithmetic_decode_string_argument(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_suffix,
    char **out_text,
    size_t *out_text_length
);
static int time_arithmetic_apply_datetime(
    struct mylite_db *database,
    const char *function_name,
    const struct scalar_time_arithmetic_input *first,
    int64_t second_seconds,
    struct session_scalar_cell *out_cell
);
static int time_arithmetic_apply_time(
    struct mylite_db *database,
    const char *function_name,
    const struct scalar_time_arithmetic_input *first,
    int64_t second_seconds,
    struct session_scalar_cell *out_cell
);
static int time_arithmetic_format_time(
    struct mylite_db *database,
    const char *function_name,
    int64_t seconds,
    struct session_scalar_cell *out_cell
);
static bool time_arithmetic_seconds_in_range(int64_t seconds);
static bool checked_int64_add(int64_t left, int64_t right, int64_t *out_result);
static bool checked_int64_negate(int64_t value, int64_t *out_result);
static int date_add_signed_integer_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    int64_t *out_value,
    bool *out_matched,
    bool *out_out_of_range
);
static bool date_add_signed_integer_literal(
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_out_of_range
);
static int date_add_signed_integer_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_matched,
    bool *out_out_of_range
);
static bool time_text_to_seconds(const char *text, size_t text_length, int64_t *out_seconds);
static bool time_text_to_components(
    const char *text,
    size_t text_length,
    bool *out_is_negative,
    uint32_t *out_hour,
    uint32_t *out_minute,
    uint32_t *out_second
);
static bool time_text_has_canonical_shape(const char *text, size_t text_length);
static bool time_text_uses_canonical_hour_width(
    const char *text,
    size_t text_length,
    const uint32_t *hour
);
static bool date_component_text_to_u32(const char *text, size_t length, uint32_t *out_value);

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

bool mylite_execution_str_to_date_child_is_null_literal(
    const struct mylite_sql_ast_node *expression
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
        size_t child_count = 0U;

        if (current == NULL) {
            continue;
        }
        if (mylite_execution_str_to_date_child_is_identifier_reference(current)) {
            scalar_temporal_node_stack_deinit(&stack);
            return mylite_execution_date_add_set_unknown_identifier_error(database, current);
        }

        child_count = mylite_sql_ast_node_child_count(current);
        for (size_t index = child_count; index > 0U; --index) {
            if (!scalar_temporal_node_stack_push(
                    &stack,
                    mylite_execution_child_at(current, index - 1U)
                )) {
                scalar_temporal_node_stack_deinit(&stack);
                mylite_execution_set_nomem_error(database);
                return MYLITE_NOMEM;
            }
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
        literal = mylite_execution_unwrap_parenthesized_expression(
            mylite_execution_child_at(literal, 0U)
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

int mylite_execution_scalar_date_format_numeric_equal_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *date_format = NULL;
    const struct mylite_sql_ast_node *numeric = NULL;
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
    if (!mylite_execution_date_format_numeric_equal_sides(expression, &date_format, &numeric)) {
        mylite_execution_set_unsupported_error(
            database,
            "DATE_FORMAT() numeric comparison supports only DATE_FORMAT(value, format) = "
            "numeric_literal"
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
        !mylite_execution_date_format_numeric_equal_format_is_supported(format, format_length)) {
        mylite_execution_set_unsupported_error(
            database,
            "DATE_FORMAT() numeric comparison supports only DATE_FORMAT(value, '%H.%i') = "
            "numeric_literal"
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
        left == right ? 1 : 0
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

bool mylite_execution_scalar_is_date_format_numeric_equal_expression(
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node *date_format = NULL;
    const struct mylite_sql_ast_node *numeric = NULL;

    return mylite_execution_date_format_numeric_equal_sides(expression, &date_format, &numeric);
}

bool mylite_execution_date_format_numeric_equal_sides(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node **out_date_format,
    const struct mylite_sql_ast_node **out_numeric
) {
    const struct mylite_sql_ast_node *left = NULL;
    const struct mylite_sql_ast_node *right = NULL;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (out_date_format == NULL || out_numeric == NULL || expression == NULL ||
        expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        mylite_sql_ast_node_operator(expression) != MYLITE_SQL_AST_OPERATOR_EQUAL) {
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
        return true;
    }
    if (right != NULL && right->kind == MYLITE_SQL_AST_DATE_FORMAT_FUNCTION &&
        date_format_numeric_literal_expression(left)) {
        *out_date_format = right;
        *out_numeric = left;
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
        literal = mylite_execution_unwrap_parenthesized_expression(
            mylite_execution_child_at(literal, 0U)
        );
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }
    return (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER ||
            mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_DECIMAL) != 0;
}

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

bool mylite_execution_date_format_numeric_equal_format_is_supported(
    const char *format,
    size_t format_length
) {
    static const char supported_format[] = "%H.%i";

    return (format != NULL && format_length == sizeof(supported_format) - 1U &&
            memcmp(format, supported_format, sizeof(supported_format) - 1U) == 0) != 0;
}

int mylite_execution_scalar_date_interval_second_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return date_interval_value(database, expression, out_cell);
}

static int date_interval_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum mylite_date_interval_unit unit = MYLITE_DATE_INTERVAL_UNIT_SECOND;
    struct mylite_temporal_datetime_parts input = {0};
    struct mylite_temporal_datetime_parts output = {0};
    const char *function_name = "DATE_ADD";
    int64_t interval_value = 0;
    bool temporal_has_time = false;
    bool temporal_is_null = false;
    bool interval_is_null = false;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL ||
        !mylite_execution_is_date_interval_second_function_kind(expression->kind)) {
        return set_date_interval_second_unsupported_shape_error(database, function_name);
    }
    function_name = mylite_execution_date_interval_second_function_name(expression->kind);
    rc = mylite_execution_validate_date_interval_second_function_shape(
        database,
        expression,
        function_name
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = date_interval_second_temporal_argument(
        database,
        function_name,
        mylite_execution_date_interval_second_temporal_node(expression),
        &input,
        &temporal_has_time,
        &temporal_is_null
    );
    if (rc != MYLITE_OK || temporal_is_null) {
        return rc;
    }
    if (expression->kind != MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION) {
        rc = mylite_execution_date_interval_unit_from_ast(
            database,
            mylite_execution_date_interval_unit_node(expression),
            function_name,
            &unit
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    rc = mylite_execution_date_interval_second_interval_argument(
        database,
        function_name,
        mylite_execution_date_interval_second_interval_node(expression),
        unit,
        &interval_value,
        &interval_is_null
    );
    if (rc != MYLITE_OK || interval_is_null) {
        return rc;
    }
    if (mylite_execution_date_interval_second_function_subtracts(expression->kind) &&
        checked_int64_negate(interval_value, &interval_value)) {
        return set_date_interval_second_unsupported_error(
            database,
            function_name,
            "result is outside the supported datetime range"
        );
    }
    rc = date_interval_apply(database, function_name, &input, interval_value, unit, &output);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return date_interval_format(
        database,
        function_name,
        &output,
        (temporal_has_time || mylite_date_interval_unit_has_time_part(unit)) != 0,
        out_cell
    );
}

bool mylite_execution_is_date_interval_second_function_kind(enum mylite_sql_ast_node_kind kind) {
    switch (kind) {
    case MYLITE_SQL_AST_DATE_ADD_FUNCTION:
    case MYLITE_SQL_AST_DATE_SUB_FUNCTION:
    case MYLITE_SQL_AST_ADDDATE_FUNCTION:
    case MYLITE_SQL_AST_SUBDATE_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION:
        return true;
    default:
        return false;
    }
}

const char *mylite_execution_date_interval_second_function_name(
    enum mylite_sql_ast_node_kind kind
) {
    switch (kind) {
    case MYLITE_SQL_AST_DATE_SUB_FUNCTION:
        return "DATE_SUB";
    case MYLITE_SQL_AST_ADDDATE_FUNCTION:
        return "ADDDATE";
    case MYLITE_SQL_AST_SUBDATE_FUNCTION:
        return "SUBDATE";
    case MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION:
        return "TIMESTAMPADD";
    case MYLITE_SQL_AST_DATE_ADD_FUNCTION:
    default:
        return "DATE_ADD";
    }
}

bool mylite_execution_date_interval_second_function_subtracts(enum mylite_sql_ast_node_kind kind) {
    return (kind == MYLITE_SQL_AST_DATE_SUB_FUNCTION || kind == MYLITE_SQL_AST_SUBDATE_FUNCTION) !=
           0;
}

int mylite_execution_validate_date_interval_second_function_shape(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name
) {
    size_t child_count = 0U;

    if (expression == NULL || function_name == NULL) {
        return MYLITE_MISUSE;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (expression->kind == MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION) {
        if (child_count != 3U) {
            return set_date_interval_second_unsupported_shape_error(database, function_name);
        }
        return timestampadd_second_unit_from_ast(
            database,
            mylite_execution_child_at(expression, 0U)
        );
    }
    if (child_count != 3U) {
        return set_date_interval_second_unsupported_shape_error(database, function_name);
    }
    return MYLITE_OK;
}

static int timestampadd_second_unit_from_ast(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *unit
) {
    char unit_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = mylite_execution_copy_identifier_text(unit, unit_name, sizeof(unit_name), database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_execution_text_equals_ascii_case_insensitive(unit_name, "SECOND") ||
        mylite_execution_text_equals_ascii_case_insensitive(unit_name, "SQL_TSI_SECOND")) {
        return MYLITE_OK;
    }
    mylite_execution_set_unsupported_error(
        database,
        "TIMESTAMPADD() supports only SECOND and SQL_TSI_SECOND units"
    );
    return MYLITE_ERROR;
}

int mylite_execution_date_interval_unit_from_ast(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *unit_node,
    const char *function_name,
    enum mylite_date_interval_unit *out_unit
) {
    char unit_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char message[date_interval_diagnostic_capacity];
    int written = 0;
    int rc = MYLITE_OK;

    if (function_name == NULL || out_unit == NULL) {
        return MYLITE_MISUSE;
    }
    *out_unit = MYLITE_DATE_INTERVAL_UNIT_SECOND;
    rc = mylite_execution_copy_identifier_text(unit_node, unit_name, sizeof(unit_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_date_interval_unit_from_name(unit_name, strlen(unit_name), out_unit) &&
        *out_unit != MYLITE_DATE_INTERVAL_UNIT_MICROSECOND) {
        return MYLITE_OK;
    }
    written = snprintf(
        message,
        sizeof(message),
        "%s() supports only YEAR, QUARTER, MONTH, WEEK, DAY, HOUR, MINUTE, and SECOND "
        "interval units",
        function_name
    );
    if (written < 0 || (size_t)written >= sizeof(message)) {
        mylite_execution_set_runtime_error(
            database,
            "failed to format temporal function diagnostic"
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_unsupported_error(database, message);
    return MYLITE_ERROR;
}

const struct mylite_sql_ast_node *mylite_execution_date_interval_second_temporal_node(
    const struct mylite_sql_ast_node *expression
) {
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION) {
        return mylite_execution_child_at(expression, 2U);
    }
    return mylite_execution_child_at(expression, 0U);
}

const struct mylite_sql_ast_node *mylite_execution_date_interval_second_interval_node(
    const struct mylite_sql_ast_node *expression
) {
    return mylite_execution_child_at(expression, 1U);
}

const struct mylite_sql_ast_node *mylite_execution_date_interval_unit_node(
    const struct mylite_sql_ast_node *expression
) {
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION) {
        return mylite_execution_child_at(expression, 0U);
    }
    return mylite_execution_child_at(expression, 2U);
}

static int set_date_interval_second_unsupported_error(
    struct mylite_db *database,
    const char *function_name,
    const char *suffix
) {
    char message[date_interval_diagnostic_capacity];

    if (!mylite_execution_date_interval_second_message(
            message,
            sizeof(message),
            function_name,
            suffix
        )) {
        mylite_execution_set_runtime_error(
            database,
            "failed to format temporal function diagnostic"
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_unsupported_error(database, message);
    return MYLITE_ERROR;
}

static int set_date_interval_second_unsupported_shape_error(
    struct mylite_db *database,
    const char *function_name
) {
    char message[date_interval_diagnostic_capacity];
    int written = snprintf(
        message,
        sizeof(message),
        "%s() supports only %s(date, INTERVAL value unit)",
        function_name,
        function_name
    );

    if (written < 0 || (size_t)written >= sizeof(message)) {
        mylite_execution_set_runtime_error(
            database,
            "failed to format temporal function diagnostic"
        );
        return MYLITE_ERROR;
    }
    mylite_execution_set_unsupported_error(database, message);
    return MYLITE_ERROR;
}

bool mylite_execution_date_interval_second_message(
    char *buffer,
    size_t buffer_size,
    const char *function_name,
    const char *suffix
) {
    int written = 0;

    if (buffer == NULL || buffer_size == 0U || function_name == NULL || suffix == NULL) {
        return false;
    }
    written = snprintf(buffer, buffer_size, "%s() %s", function_name, suffix);
    return (written >= 0 && (size_t)written < buffer_size) != 0;
}

static int date_interval_second_temporal_argument(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *expression,
    struct mylite_temporal_datetime_parts *out_datetime,
    bool *out_has_time,
    bool *out_is_null
) {
    char unsupported_message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    char nul_message[date_interval_nul_diagnostic_capacity];
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (out_datetime == NULL || out_has_time == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_datetime = (struct mylite_temporal_datetime_parts){0};
    *out_has_time = false;
    *out_is_null = false;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (!mylite_execution_date_interval_second_message(
            unsupported_message,
            sizeof(unsupported_message),
            function_name,
            "supports only date or datetime string literals and NULL"
        ) ||
        !mylite_execution_date_interval_second_message(
            nul_message,
            sizeof(nul_message),
            function_name,
            "date literals do not support NUL bytes"
        )) {
        mylite_execution_set_runtime_error(
            database,
            "failed to format temporal function diagnostic"
        );
        return MYLITE_ERROR;
    }
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER) {
        return mylite_execution_date_add_set_unknown_identifier_error(database, expression);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        mylite_execution_set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    rc = mylite_execution_decode_sql_string_literal(
        database,
        expression,
        unsupported_message,
        nul_message,
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && memchr(text, '\0', text_length) != NULL) {
        mylite_execution_set_unsupported_error(database, nul_message);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK &&
        !mylite_temporal_arithmetic_parse_datetime_text(text, text_length, out_datetime)) {
        rc = set_date_interval_second_unsupported_error(
            database,
            function_name,
            "supports only canonical YYYY-MM-DD or YYYY-MM-DD HH:MM:SS values"
        );
    }
    if (rc == MYLITE_OK) {
        *out_has_time = text_length == datetime_text_length;
    }
    free(text);
    return rc;
}

int mylite_execution_date_interval_second_interval_argument(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *expression,
    enum mylite_date_interval_unit unit,
    int64_t *out_interval,
    bool *out_is_null
) {
    bool interval_matched = false;
    bool out_of_range = false;
    int rc = MYLITE_OK;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (out_interval == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_interval = 0;
    *out_is_null = false;

    if (expression == NULL) {
        return set_date_interval_argument_support_error(database, function_name, unit);
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    rc = date_add_signed_integer_expression(
        database,
        expression,
        function_name,
        out_interval,
        &interval_matched,
        &out_of_range
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!interval_matched) {
        if (out_of_range) {
            return set_date_interval_argument_range_error(database, function_name, unit);
        }
        return set_date_interval_argument_support_error(database, function_name, unit);
    }

    return MYLITE_OK;
}

static const char *date_interval_literal_support_text(const char *function_name) {
    if (function_name != NULL && strcmp(function_name, "TIMESTAMPADD") == 0) {
        return "signed integer literals and NULL";
    }
    return "signed integer literals, exact signed integer string literals, and NULL";
}

static int set_date_interval_argument_support_error(
    struct mylite_db *database,
    const char *function_name,
    enum mylite_date_interval_unit unit
) {
    const char *literal_support = date_interval_literal_support_text(function_name);
    const char *unit_name = mylite_date_interval_unit_name(unit);
    char message[date_interval_diagnostic_capacity];
    int written = 0;

    if (unit_name == NULL) {
        unit_name = "unit";
    }
    written = snprintf(
        message,
        sizeof(message),
        "INTERVAL %s supports only %s",
        unit_name,
        literal_support
    );
    if (written < 0 || (size_t)written >= sizeof(message)) {
        mylite_execution_set_runtime_error(
            database,
            "failed to format temporal function diagnostic"
        );
        return MYLITE_ERROR;
    }
    return set_date_interval_second_unsupported_error(database, function_name, message);
}

static int set_date_interval_argument_range_error(
    struct mylite_db *database,
    const char *function_name,
    enum mylite_date_interval_unit unit
) {
    const char *unit_name = mylite_date_interval_unit_name(unit);
    char message[date_interval_diagnostic_capacity];
    int written = 0;

    if (unit_name == NULL) {
        unit_name = "unit";
    }
    written = snprintf(
        message,
        sizeof(message),
        "INTERVAL %s literals must fit the signed 64-bit range",
        unit_name
    );
    if (written < 0 || (size_t)written >= sizeof(message)) {
        mylite_execution_set_runtime_error(
            database,
            "failed to format temporal function diagnostic"
        );
        return MYLITE_ERROR;
    }
    return set_date_interval_second_unsupported_error(database, function_name, message);
}

static int date_interval_apply(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_temporal_datetime_parts *input,
    int64_t interval_value,
    enum mylite_date_interval_unit unit,
    struct mylite_temporal_datetime_parts *out_datetime
) {
    int64_t interval_seconds = 0;

    if (input == NULL || out_datetime == NULL) {
        return MYLITE_MISUSE;
    }
    switch (unit) {
    case MYLITE_DATE_INTERVAL_UNIT_YEAR:
        if (!mylite_temporal_arithmetic_checked_multiply_int64(
                interval_value,
                date_months_per_year,
                &interval_seconds
            )) {
            return set_date_interval_second_unsupported_error(
                database,
                function_name,
                "result is outside the supported datetime range"
            );
        }
        return date_interval_apply_calendar_months(
            database,
            function_name,
            input,
            interval_seconds,
            out_datetime
        );
    case MYLITE_DATE_INTERVAL_UNIT_QUARTER:
        if (!mylite_temporal_arithmetic_checked_multiply_int64(
                interval_value,
                date_interval_months_per_quarter,
                &interval_seconds
            )) {
            return set_date_interval_second_unsupported_error(
                database,
                function_name,
                "result is outside the supported datetime range"
            );
        }
        return date_interval_apply_calendar_months(
            database,
            function_name,
            input,
            interval_seconds,
            out_datetime
        );
    case MYLITE_DATE_INTERVAL_UNIT_MONTH:
        return date_interval_apply_calendar_months(
            database,
            function_name,
            input,
            interval_value,
            out_datetime
        );
    case MYLITE_DATE_INTERVAL_UNIT_WEEK:
        if (!mylite_temporal_arithmetic_checked_multiply_int64(
                interval_value,
                mylite_temporal_arithmetic_seconds_per_day() * date_interval_days_per_week,
                &interval_seconds
            )) {
            return set_date_interval_second_unsupported_error(
                database,
                function_name,
                "result is outside the supported datetime range"
            );
        }
        return date_interval_second_apply(
            database,
            function_name,
            input,
            interval_seconds,
            out_datetime
        );
    case MYLITE_DATE_INTERVAL_UNIT_DAY:
        if (!mylite_temporal_arithmetic_checked_multiply_int64(
                interval_value,
                mylite_temporal_arithmetic_seconds_per_day(),
                &interval_seconds
            )) {
            return set_date_interval_second_unsupported_error(
                database,
                function_name,
                "result is outside the supported datetime range"
            );
        }
        return date_interval_second_apply(
            database,
            function_name,
            input,
            interval_seconds,
            out_datetime
        );
    case MYLITE_DATE_INTERVAL_UNIT_HOUR:
        if (!mylite_temporal_arithmetic_checked_multiply_int64(
                interval_value,
                time_second_per_hour,
                &interval_seconds
            )) {
            return set_date_interval_second_unsupported_error(
                database,
                function_name,
                "result is outside the supported datetime range"
            );
        }
        return date_interval_second_apply(
            database,
            function_name,
            input,
            interval_seconds,
            out_datetime
        );
    case MYLITE_DATE_INTERVAL_UNIT_MINUTE:
        if (!mylite_temporal_arithmetic_checked_multiply_int64(
                interval_value,
                time_second_per_minute,
                &interval_seconds
            )) {
            return set_date_interval_second_unsupported_error(
                database,
                function_name,
                "result is outside the supported datetime range"
            );
        }
        return date_interval_second_apply(
            database,
            function_name,
            input,
            interval_seconds,
            out_datetime
        );
    case MYLITE_DATE_INTERVAL_UNIT_SECOND:
        return date_interval_second_apply(
            database,
            function_name,
            input,
            interval_value,
            out_datetime
        );
    case MYLITE_DATE_INTERVAL_UNIT_MICROSECOND:
    default:
        return set_date_interval_second_unsupported_error(
            database,
            function_name,
            "supports only YEAR, QUARTER, MONTH, WEEK, DAY, HOUR, MINUTE, and SECOND interval "
            "units"
        );
    }
}

static int date_interval_apply_calendar_months(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_temporal_datetime_parts *input,
    int64_t interval_months,
    struct mylite_temporal_datetime_parts *out_datetime
) {
    bool applied =
        mylite_temporal_arithmetic_add_calendar_months(input, interval_months, out_datetime);

    if (applied) {
        return MYLITE_OK;
    }
    return set_date_interval_second_unsupported_error(
        database,
        function_name,
        "result is outside the supported datetime range"
    );
}

static int date_interval_second_apply(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_temporal_datetime_parts *input,
    int64_t interval_seconds,
    struct mylite_temporal_datetime_parts *out_datetime
) {
    const int64_t seconds_per_day = mylite_temporal_arithmetic_seconds_per_day();
    struct mylite_temporal_day_second result_day_second = {0};
    int64_t days = 0;
    int64_t day_seconds = 0;
    int64_t base_seconds = 0;
    int64_t result_seconds = 0;
    int64_t result_day_seconds = 0;

    if (input == NULL || out_datetime == NULL) {
        return MYLITE_MISUSE;
    }
    *out_datetime = (struct mylite_temporal_datetime_parts){0};

    days = mylite_temporal_arithmetic_days_from_datetime(input);
    day_seconds = ((int64_t)input->hour * (int64_t)time_second_per_hour) +
                  ((int64_t)input->minute * (int64_t)time_second_per_minute) +
                  (int64_t)input->second;
    base_seconds = (days * seconds_per_day) + day_seconds;
    if (!mylite_temporal_arithmetic_checked_add_int64(
            base_seconds,
            interval_seconds,
            &result_seconds
        )) {
        return set_date_interval_second_unsupported_error(
            database,
            function_name,
            "result is outside the supported datetime range"
        );
    }

    result_day_second = mylite_temporal_arithmetic_floor_divmod_seconds(result_seconds);
    mylite_temporal_arithmetic_civil_from_days(result_day_second.days, out_datetime);
    if (out_datetime->year < date_minimum_year || out_datetime->year > date_maximum_year) {
        return set_date_interval_second_unsupported_error(
            database,
            function_name,
            "result is outside the supported datetime range"
        );
    }

    result_day_seconds = result_day_second.day_seconds;
    out_datetime->hour = (uint32_t)(result_day_seconds / time_second_per_hour);
    result_day_seconds %= time_second_per_hour;
    out_datetime->minute = (uint32_t)(result_day_seconds / time_second_per_minute);
    out_datetime->second = (uint32_t)(result_day_seconds % time_second_per_minute);
    return MYLITE_OK;
}

static int date_interval_second_format(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_temporal_datetime_parts *datetime,
    struct session_scalar_cell *out_cell
) {
    return date_interval_format(database, function_name, datetime, true, out_cell);
}

static int date_interval_format(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_temporal_datetime_parts *datetime,
    bool result_has_time,
    struct session_scalar_cell *out_cell
) {
    char message[date_interval_format_diagnostic_capacity];
    int expected_length = date_text_length;
    int written = 0;

    if (datetime == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }

    if (result_has_time) {
        expected_length = datetime_text_length;
        written = snprintf(
            out_cell->datetime_text,
            sizeof(out_cell->datetime_text),
            "%04" PRId64 "-%02" PRIu32 "-%02" PRIu32 " %02" PRIu32 ":%02" PRIu32 ":%02" PRIu32,
            datetime->year,
            datetime->month,
            datetime->day,
            datetime->hour,
            datetime->minute,
            datetime->second
        );
    } else {
        written = snprintf(
            out_cell->datetime_text,
            sizeof(out_cell->datetime_text),
            "%04" PRId64 "-%02" PRIu32 "-%02" PRIu32,
            datetime->year,
            datetime->month,
            datetime->day
        );
    }
    if (written != expected_length) {
        written = snprintf(message, sizeof(message), "failed to format %s() result", function_name);
        if (written < 0 || (size_t)written >= sizeof(message)) {
            mylite_execution_set_runtime_error(
                database,
                "failed to format temporal function result"
            );
            return MYLITE_ERROR;
        }
        mylite_execution_set_runtime_error(database, message);
        return MYLITE_ERROR;
    }

    out_cell->value = out_cell->datetime_text;
    return MYLITE_OK;
}

int mylite_execution_scalar_addtime_subtime_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct scalar_time_arithmetic_input first = {0};
    struct scalar_time_arithmetic_input second = {0};
    const char *function_name = "ADDTIME";
    int64_t second_seconds = 0;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL ||
        !mylite_execution_is_time_arithmetic_function_kind(expression->kind) ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        mylite_execution_set_native_function_parameter_count_error(database, function_name);
        return MYLITE_ERROR;
    }
    function_name = time_arithmetic_function_name(expression->kind);

    rc = time_arithmetic_first_argument(
        database,
        function_name,
        mylite_execution_child_at(expression, 0U),
        &first
    );
    if (rc != MYLITE_OK || first.kind == SCALAR_TIME_ARITHMETIC_INPUT_NULL) {
        return rc;
    }
    rc = time_arithmetic_second_argument(
        database,
        function_name,
        mylite_execution_child_at(expression, 1U),
        &second
    );
    if (rc != MYLITE_OK || second.kind == SCALAR_TIME_ARITHMETIC_INPUT_NULL) {
        return rc;
    }

    second_seconds = second.time_seconds;
    if (time_arithmetic_function_subtracts(expression->kind) &&
        checked_int64_negate(second_seconds, &second_seconds)) {
        return set_time_arithmetic_unsupported_error(
            database,
            function_name,
            "result is outside the supported time or datetime range"
        );
    }
    if (first.kind == SCALAR_TIME_ARITHMETIC_INPUT_DATETIME) {
        return time_arithmetic_apply_datetime(
            database,
            function_name,
            &first,
            second_seconds,
            out_cell
        );
    }
    return time_arithmetic_apply_time(database, function_name, &first, second_seconds, out_cell);
}

bool mylite_execution_is_time_arithmetic_function_kind(enum mylite_sql_ast_node_kind kind) {
    switch (kind) {
    case MYLITE_SQL_AST_ADDTIME_FUNCTION:
    case MYLITE_SQL_AST_SUBTIME_FUNCTION:
        return true;
    default:
        return false;
    }
}

static const char *time_arithmetic_function_name(enum mylite_sql_ast_node_kind kind) {
    switch (kind) {
    case MYLITE_SQL_AST_SUBTIME_FUNCTION:
        return "SUBTIME";
    case MYLITE_SQL_AST_ADDTIME_FUNCTION:
    default:
        return "ADDTIME";
    }
}

static bool time_arithmetic_function_subtracts(enum mylite_sql_ast_node_kind kind) {
    return kind == MYLITE_SQL_AST_SUBTIME_FUNCTION;
}

static int set_time_arithmetic_unsupported_error(
    struct mylite_db *database,
    const char *function_name,
    const char *suffix
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

    if (!time_arithmetic_message(message, sizeof(message), function_name, suffix)) {
        mylite_execution_set_runtime_error(database, "failed to format time arithmetic diagnostic");
        return MYLITE_ERROR;
    }
    mylite_execution_set_unsupported_error(database, message);
    return MYLITE_ERROR;
}

static bool time_arithmetic_message(
    char *buffer,
    size_t buffer_size,
    const char *function_name,
    const char *suffix
) {
    int written = 0;

    if (buffer == NULL || buffer_size == 0U || function_name == NULL || suffix == NULL) {
        return false;
    }
    written = snprintf(buffer, buffer_size, "%s() %s", function_name, suffix);
    return (written >= 0 && (size_t)written < buffer_size) != 0;
}

static int time_arithmetic_first_argument(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *expression,
    struct scalar_time_arithmetic_input *out_input
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (out_input == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input = (struct scalar_time_arithmetic_input){0};

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return set_time_arithmetic_unsupported_error(
            database,
            function_name,
            "supports only canonical datetime string literals, canonical time string literals, "
            "and NULL"
        );
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return mylite_execution_date_add_set_unknown_identifier_error(database, expression);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        return set_time_arithmetic_unsupported_error(
            database,
            function_name,
            "supports only canonical datetime string literals, canonical time string literals, "
            "and NULL"
        );
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_input->kind = SCALAR_TIME_ARITHMETIC_INPUT_NULL;
        return MYLITE_OK;
    }

    rc = time_arithmetic_decode_string_argument(
        database,
        function_name,
        expression,
        "supports only canonical datetime string literals, canonical time string literals, "
        "and NULL",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && text_length == datetime_text_length &&
        mylite_temporal_arithmetic_parse_datetime_text(text, text_length, &out_input->datetime)) {
        out_input->kind = SCALAR_TIME_ARITHMETIC_INPUT_DATETIME;
    } else if (
        rc == MYLITE_OK && time_text_to_seconds(text, text_length, &out_input->time_seconds)
    ) {
        out_input->kind = SCALAR_TIME_ARITHMETIC_INPUT_TIME;
    } else if (rc == MYLITE_OK) {
        rc = set_time_arithmetic_unsupported_error(
            database,
            function_name,
            "supports only canonical YYYY-MM-DD HH:MM:SS datetime or canonical [-]HH:MM:SS time "
            "values"
        );
    }

    free(text);
    return rc;
}

static int time_arithmetic_second_argument(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *expression,
    struct scalar_time_arithmetic_input *out_input
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (out_input == NULL) {
        return MYLITE_MISUSE;
    }
    *out_input = (struct scalar_time_arithmetic_input){0};

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return set_time_arithmetic_unsupported_error(
            database,
            function_name,
            "time argument supports only canonical time string literals and NULL"
        );
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return mylite_execution_date_add_set_unknown_identifier_error(database, expression);
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        return set_time_arithmetic_unsupported_error(
            database,
            function_name,
            "time argument supports only canonical time string literals and NULL"
        );
    }
    if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_NULL) {
        out_input->kind = SCALAR_TIME_ARITHMETIC_INPUT_NULL;
        return MYLITE_OK;
    }

    rc = time_arithmetic_decode_string_argument(
        database,
        function_name,
        expression,
        "time argument supports only canonical time string literals and NULL",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && time_text_to_seconds(text, text_length, &out_input->time_seconds)) {
        out_input->kind = SCALAR_TIME_ARITHMETIC_INPUT_TIME;
    } else if (rc == MYLITE_OK) {
        rc = set_time_arithmetic_unsupported_error(
            database,
            function_name,
            "time argument supports only canonical [-]HH:MM:SS time values"
        );
    }

    free(text);
    return rc;
}

static int time_arithmetic_decode_string_argument(
    struct mylite_db *database,
    const char *function_name,
    const struct mylite_sql_ast_node *expression,
    const char *unsupported_suffix,
    char **out_text,
    size_t *out_text_length
) {
    char unsupported_message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    char nul_message[date_interval_nul_diagnostic_capacity];
    int rc = MYLITE_OK;

    if (out_text == NULL || out_text_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
        return set_time_arithmetic_unsupported_error(database, function_name, unsupported_suffix);
    }
    if (!time_arithmetic_message(
            unsupported_message,
            sizeof(unsupported_message),
            function_name,
            unsupported_suffix
        ) ||
        !time_arithmetic_message(
            nul_message,
            sizeof(nul_message),
            function_name,
            "time literals do not support NUL bytes"
        )) {
        mylite_execution_set_runtime_error(database, "failed to format time arithmetic diagnostic");
        return MYLITE_ERROR;
    }

    rc = mylite_execution_decode_sql_string_literal(
        database,
        expression,
        unsupported_message,
        nul_message,
        out_text,
        out_text_length
    );
    if (rc == MYLITE_OK && memchr(*out_text, '\0', *out_text_length) != NULL) {
        mylite_execution_set_unsupported_error(database, nul_message);
        free(*out_text);
        *out_text = NULL;
        *out_text_length = 0U;
        return MYLITE_ERROR;
    }
    return rc;
}

static int time_arithmetic_apply_datetime(
    struct mylite_db *database,
    const char *function_name,
    const struct scalar_time_arithmetic_input *first,
    int64_t second_seconds,
    struct session_scalar_cell *out_cell
) {
    struct mylite_temporal_datetime_parts output = {0};
    int rc = MYLITE_OK;

    if (first == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    rc = date_interval_second_apply(
        database,
        function_name,
        &first->datetime,
        second_seconds,
        &output
    );
    if (rc != MYLITE_OK) {
        return set_time_arithmetic_unsupported_error(
            database,
            function_name,
            "result is outside the supported time or datetime range"
        );
    }
    return date_interval_second_format(database, function_name, &output, out_cell);
}

static int time_arithmetic_apply_time(
    struct mylite_db *database,
    const char *function_name,
    const struct scalar_time_arithmetic_input *first,
    int64_t second_seconds,
    struct session_scalar_cell *out_cell
) {
    int64_t result_seconds = 0;

    if (first == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (checked_int64_add(first->time_seconds, second_seconds, &result_seconds) ||
        !time_arithmetic_seconds_in_range(result_seconds)) {
        return set_time_arithmetic_unsupported_error(
            database,
            function_name,
            "result is outside the supported time or datetime range"
        );
    }
    return time_arithmetic_format_time(database, function_name, result_seconds, out_cell);
}

static int time_arithmetic_format_time(
    struct mylite_db *database,
    const char *function_name,
    int64_t seconds,
    struct session_scalar_cell *out_cell
) {
    char buffer[sizeof("-838:59:59")];
    bool is_negative = seconds < 0;
    int64_t magnitude = seconds;
    int64_t hour = 0;
    int64_t minute = 0;
    int64_t second = 0;
    int written = 0;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (is_negative) {
        magnitude = -magnitude;
    }
    hour = magnitude / time_second_per_hour;
    magnitude %= time_second_per_hour;
    minute = magnitude / time_second_per_minute;
    second = magnitude % time_second_per_minute;
    written = snprintf(
        buffer,
        sizeof(buffer),
        "%s%02" PRId64 ":%02" PRId64 ":%02" PRId64,
        is_negative ? "-" : "",
        hour,
        minute,
        second
    );
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        char message[date_interval_format_diagnostic_capacity];

        written = snprintf(message, sizeof(message), "failed to format %s() result", function_name);
        if (written < 0 || (size_t)written >= sizeof(message)) {
            mylite_execution_set_runtime_error(database, "failed to format time arithmetic result");
            return MYLITE_ERROR;
        }
        mylite_execution_set_runtime_error(database, message);
        return MYLITE_ERROR;
    }

    out_cell->owned_text = (char *)malloc((size_t)written + 1U);
    if (out_cell->owned_text == NULL) {
        mylite_execution_set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(out_cell->owned_text, buffer, (size_t)written + 1U);
    out_cell->value = out_cell->owned_text;
    return MYLITE_OK;
}

static bool time_arithmetic_seconds_in_range(int64_t seconds) {
    const int64_t maximum = ((int64_t)time_maximum_hour * time_second_per_hour) +
                            ((int64_t)time_maximum_minute_or_second * time_second_per_minute) +
                            (int64_t)time_maximum_minute_or_second;

    return (seconds >= -maximum && seconds <= maximum) != 0;
}

static int date_add_signed_integer_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *function_name,
    int64_t *out_value,
    bool *out_matched,
    bool *out_out_of_range
) {
    if (out_value == NULL || out_matched == NULL || out_out_of_range == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matched = false;
    if (date_add_signed_integer_literal(expression, out_value, out_out_of_range)) {
        *out_matched = true;
        return MYLITE_OK;
    }
    if (*out_out_of_range) {
        return MYLITE_OK;
    }
    if (function_name != NULL && strcmp(function_name, "TIMESTAMPADD") == 0) {
        return MYLITE_OK;
    }
    return date_add_signed_integer_string_literal(
        database,
        expression,
        out_value,
        out_matched,
        out_out_of_range
    );
}

static bool date_add_signed_integer_literal(
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_out_of_range
) {
    const uint64_t signed_negative_abs_max = 9223372036854775808ULL;
    const struct mylite_sql_ast_node *literal = expression;
    uint64_t magnitude = 0U;
    bool is_negative = false;

    if (out_value == NULL || out_out_of_range == NULL) {
        return false;
    }
    *out_value = 0;
    *out_out_of_range = false;
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            return false;
        }
        literal = mylite_execution_unwrap_parenthesized_expression(
            mylite_execution_child_at(expression, 0U)
        );
    } else {
        literal = expression;
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return false;
    }
    if (mylite_execution_parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        *out_out_of_range = true;
        return false;
    }

    if (is_negative) {
        if (magnitude > signed_negative_abs_max) {
            *out_out_of_range = true;
            return false;
        }
        if (magnitude == signed_negative_abs_max) {
            *out_value = INT64_MIN;
        } else {
            *out_value = -(int64_t)magnitude;
        }
        return true;
    }
    if (magnitude > (uint64_t)INT64_MAX) {
        *out_out_of_range = true;
        return false;
    }
    *out_value = (int64_t)magnitude;
    return true;
}

static int date_add_signed_integer_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    int64_t *out_value,
    bool *out_matched,
    bool *out_out_of_range
) {
    const uint64_t signed_negative_abs_max = 9223372036854775808ULL;
    const char unsupported_message[] = "DATE interval string literal expected";
    const char nul_message[] = "DATE interval string literal does not support NUL bytes";
    char *text = NULL;
    const char *digits = NULL;
    size_t digit_count = 0U;
    size_t text_length = 0U;
    uint64_t magnitude = 0U;
    uint64_t limit = (uint64_t)INT64_MAX;
    bool is_negative = false;
    bool is_positive = false;
    int rc = MYLITE_OK;

    if (out_value == NULL || out_matched == NULL || out_out_of_range == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0;
    *out_matched = false;
    *out_out_of_range = false;
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(expression) != MYLITE_SQL_AST_LITERAL_STRING) {
        return MYLITE_OK;
    }
    rc = mylite_execution_decode_sql_string_literal(
        database,
        expression,
        unsupported_message,
        nul_message,
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        free(text);
        return rc;
    }
    if (text_length == 0U) {
        free(text);
        return MYLITE_OK;
    }
    is_negative = text[0] == '-';
    is_positive = text[0] == '+';
    digits = text + (is_negative || is_positive ? 1U : 0U);
    digit_count = text_length - (is_negative || is_positive ? 1U : 0U);
    if (digit_count == 0U) {
        free(text);
        return MYLITE_OK;
    }
    if (is_negative) {
        limit = signed_negative_abs_max;
    }
    for (size_t digit_index = 0U; digit_index < digit_count; ++digit_index) {
        unsigned char byte = (unsigned char)digits[digit_index];
        uint64_t digit = 0U;

        if (byte < '0' || byte > '9') {
            free(text);
            return MYLITE_OK;
        }
        digit = (uint64_t)(byte - '0');
        if (magnitude > (limit - digit) / decimal_base) {
            *out_out_of_range = true;
            free(text);
            return MYLITE_OK;
        }
        magnitude = (magnitude * decimal_base) + digit;
    }
    if (is_negative) {
        *out_value = magnitude == signed_negative_abs_max ? INT64_MIN : -(int64_t)magnitude;
    } else {
        *out_value = (int64_t)magnitude;
    }
    *out_matched = true;
    free(text);
    return MYLITE_OK;
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

static bool checked_int64_add(int64_t left, int64_t right, int64_t *out_result) {
    if ((right > 0 && left > INT64_MAX - right) || (right < 0 && left < INT64_MIN - right)) {
        return true;
    }
    *out_result = left + right;
    return false;
}

static bool checked_int64_negate(int64_t value, int64_t *out_result) {
    if (value == INT64_MIN) {
        return true;
    }
    *out_result = -value;
    return false;
}

static bool time_text_to_seconds(const char *text, size_t text_length, int64_t *out_seconds) {
    bool is_negative = false;
    uint32_t hour = 0U;
    uint32_t minute = 0U;
    uint32_t second = 0U;
    int64_t total = 0;

    if (out_seconds == NULL ||
        !time_text_to_components(text, text_length, &is_negative, &hour, &minute, &second)) {
        return false;
    }
    if (minute > time_maximum_minute_or_second || second > time_maximum_minute_or_second) {
        return false;
    }
    if (!time_text_uses_canonical_hour_width(text, text_length, &hour)) {
        return false;
    }
    if (hour == 0U && minute == 0U && second == 0U && is_negative) {
        return false;
    }
    if (hour > time_maximum_hour ||
        (hour == time_maximum_hour &&
         (minute > time_maximum_minute_or_second || second > time_maximum_minute_or_second))) {
        return false;
    }

    total = ((int64_t)hour * time_second_per_hour) + ((int64_t)minute * time_second_per_minute) +
            (int64_t)second;
    if (is_negative) {
        total = -total;
    }
    *out_seconds = total;
    return true;
}

static bool time_text_to_components(
    const char *text,
    size_t text_length,
    bool *out_is_negative,
    uint32_t *out_hour,
    uint32_t *out_minute,
    uint32_t *out_second
) {
    bool is_negative = false;
    size_t hour_offset = 0U;
    size_t first_separator = 0U;
    size_t second_separator = 0U;

    if (out_is_negative == NULL || out_hour == NULL || out_minute == NULL || out_second == NULL ||
        !time_text_has_canonical_shape(text, text_length)) {
        return false;
    }
    is_negative = text[0] == '-';
    if (is_negative) {
        hour_offset = 1U;
    }
    first_separator = text_length - time_minute_second_suffix_length;
    second_separator = text_length - 3U;
    if (!date_component_text_to_u32(text + hour_offset, first_separator - hour_offset, out_hour) ||
        !date_component_text_to_u32(text + first_separator + 1U, 2U, out_minute) ||
        !date_component_text_to_u32(text + second_separator + 1U, 2U, out_second)) {
        return false;
    }

    *out_is_negative = is_negative;
    return true;
}

static bool time_text_has_canonical_shape(const char *text, size_t text_length) {
    bool is_negative = false;
    size_t hour_offset = 0U;
    size_t first_separator = 0U;
    size_t second_separator = 0U;

    if (text == NULL || text_length < time_text_minimum_length ||
        text_length > time_text_maximum_length) {
        return false;
    }
    is_negative = text[0] == '-';
    if (is_negative) {
        hour_offset = 1U;
    }
    if (is_negative && text_length == time_text_minimum_length) {
        return false;
    }
    first_separator = text_length - time_minute_second_suffix_length;
    second_separator = text_length - 3U;
    if (first_separator <= hour_offset || text[first_separator] != ':' ||
        text[second_separator] != ':') {
        return false;
    }
    if (first_separator - hour_offset < 2U || first_separator - hour_offset > 3U) {
        return false;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if ((index == 0U && is_negative) || index == first_separator || index == second_separator) {
            continue;
        }
        if (byte < '0' || byte > '9') {
            return false;
        }
    }

    return true;
}

static bool time_text_uses_canonical_hour_width(
    const char *text,
    size_t text_length,
    const uint32_t *hour
) {
    bool is_negative = false;
    size_t hour_offset = 0U;
    size_t first_separator = 0U;
    size_t hour_digit_count = 0U;

    if (hour == NULL || !time_text_has_canonical_shape(text, text_length)) {
        return false;
    }

    is_negative = text[0] == '-';
    if (is_negative) {
        hour_offset = 1U;
    }
    first_separator = text_length - time_minute_second_suffix_length;
    hour_digit_count = first_separator - hour_offset;

    return (hour_digit_count == 2U ||
            (hour_digit_count == 3U && *hour >= time_minimum_three_digit_hour)) != 0;
}

static bool date_component_text_to_u32(const char *text, size_t length, uint32_t *out_value) {
    uint32_t value = 0U;

    if (text == NULL || out_value == NULL || length == 0U) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (byte < '0' || byte > '9') {
            return false;
        }
        value = (value * decimal_base) + (uint32_t)(byte - '0');
    }
    *out_value = value;
    return true;
}
