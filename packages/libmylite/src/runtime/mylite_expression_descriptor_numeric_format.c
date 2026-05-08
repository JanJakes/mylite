#include "mylite_expression_descriptor_numeric_format.h"

#include "mylite_expression_descriptor.h"
#include "mylite_expression_descriptor_numeric.h"
#include "mylite_function_names.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

#include <stdint.h>
#include <string.h>

static uint64_t format_function_result_character_length(
    const struct mylite_sql_ast_node *argument,
    const struct mylite_field_descriptor *argument_descriptor
);

// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t format_decimal_literal_result_character_length(
    const struct mylite_sql_ast_node *argument
);

static uint64_t format_decimal_literal_descriptor_length(
    const struct mylite_sql_ast_node *argument
);

static uint64_t format_decimal_result_character_length(uint64_t decimal_length);

// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t format_literal_result_character_length(const struct mylite_sql_ast_node *argument);

int mylite_expression_descriptor_infer_format_function(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_numeric_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *value_argument = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor value_descriptor = mylite_expression_descriptor_defaults();
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);
    uint64_t character_length = 0U;
    int status = MYLITE_OK;

    if (!mylite_function_name_is_format(name)) {
        return MYLITE_UNSUPPORTED;
    }
    status =
        callbacks
            ->infer_expression_descriptor(database, plan, value_argument, NULL, &value_descriptor);
    if (status != MYLITE_OK) {
        return status;
    }

    character_length = format_function_result_character_length(value_argument, &value_descriptor);
    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .length = character_length * max_bytes_per_character,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_charset_id(database),
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return MYLITE_OK;
}

static uint64_t format_function_result_character_length(
    const struct mylite_sql_ast_node *argument,
    const struct mylite_field_descriptor *argument_descriptor
) {
    uint64_t decimal_literal_length = format_decimal_literal_result_character_length(argument);
    uint64_t literal_length = format_literal_result_character_length(argument);

    if (decimal_literal_length != 0U) {
        return decimal_literal_length;
    }
    if (literal_length != 0U) {
        return literal_length;
    }
    if (argument_descriptor == NULL || argument_descriptor->type == MYLITE_FIELD_TYPE_NULL) {
        return mylite_format_null_character_length;
    }
    switch (argument_descriptor->type) {
    case MYLITE_FIELD_TYPE_TINY:
    case MYLITE_FIELD_TYPE_SHORT:
    case MYLITE_FIELD_TYPE_LONG:
    case MYLITE_FIELD_TYPE_LONGLONG:
    case MYLITE_FIELD_TYPE_INT24:
    case MYLITE_FIELD_TYPE_YEAR:
        return argument_descriptor->length + mylite_format_numeric_descriptor_extra_length;
    case MYLITE_FIELD_TYPE_NEWDECIMAL:
        return format_decimal_result_character_length(argument_descriptor->length);
    case MYLITE_FIELD_TYPE_FLOAT:
    case MYLITE_FIELD_TYPE_DOUBLE:
        return mylite_format_float_character_length;
    case MYLITE_FIELD_TYPE_STRING:
    case MYLITE_FIELD_TYPE_VAR_STRING:
    case MYLITE_FIELD_TYPE_VARCHAR:
    case MYLITE_FIELD_TYPE_TINY_BLOB:
    case MYLITE_FIELD_TYPE_MEDIUM_BLOB:
    case MYLITE_FIELD_TYPE_LONG_BLOB:
    case MYLITE_FIELD_TYPE_BLOB:
    case MYLITE_FIELD_TYPE_JSON:
        return argument_descriptor->length + mylite_format_string_descriptor_extra_length;
    case MYLITE_FIELD_TYPE_DECIMAL:
    case MYLITE_FIELD_TYPE_NULL:
    case MYLITE_FIELD_TYPE_TIMESTAMP:
    case MYLITE_FIELD_TYPE_DATE:
    case MYLITE_FIELD_TYPE_TIME:
    case MYLITE_FIELD_TYPE_DATETIME:
    case MYLITE_FIELD_TYPE_BIT:
    case MYLITE_FIELD_TYPE_NEWDATE:
    case MYLITE_FIELD_TYPE_ENUM:
    case MYLITE_FIELD_TYPE_SET:
    case MYLITE_FIELD_TYPE_GEOMETRY:
    default:
        return mylite_format_string_descriptor_extra_length;
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t format_decimal_literal_result_character_length(
    const struct mylite_sql_ast_node *argument
) {
    uint64_t descriptor_length = format_decimal_literal_descriptor_length(argument);

    return descriptor_length == 0U ? 0U : format_decimal_result_character_length(descriptor_length);
}

static uint64_t format_decimal_literal_descriptor_length(
    const struct mylite_sql_ast_node *argument
) {
    const char *text = NULL;
    const char *dot = NULL;
    const char *end = NULL;
    uint64_t length = 0U;

    argument = mylite_sql_ast_unwrap_parenthesized_expression(argument);
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (argument->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        return format_decimal_literal_descriptor_length(mylite_ast_child_at(argument, 0U));
    }
    if (argument == NULL || argument->kind != MYLITE_SQL_AST_LITERAL ||
        argument->literal_kind != MYLITE_SQL_AST_LITERAL_DECIMAL) {
        return 0U;
    }
    text = argument->span.text;
    end = text == NULL ? NULL : text + argument->span.length;
    dot = text == NULL ? NULL : memchr(text, '.', argument->span.length);
    if (dot == NULL) {
        return 0U;
    }
    length = argument->span.length;
    if (dot == text || (dot > text && dot + 1 < end)) {
        ++length;
    }
    return length;
}

static uint64_t format_decimal_result_character_length(uint64_t decimal_length) {
    uint64_t grouping_slack = decimal_length > 3U ? (decimal_length - 3U) / 3U : 0U;
    uint64_t total = 0U;

    if (decimal_length > UINT64_MAX - mylite_format_decimal_literal_extra_length) {
        return UINT64_MAX;
    }
    total = decimal_length + mylite_format_decimal_literal_extra_length;
    if (grouping_slack > UINT64_MAX - total) {
        return UINT64_MAX;
    }
    return total + grouping_slack;
}

// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t format_literal_result_character_length(const struct mylite_sql_ast_node *argument) {
    argument = mylite_sql_ast_unwrap_parenthesized_expression(argument);
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (argument->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        return format_literal_result_character_length(mylite_ast_child_at(argument, 0U));
    }
    if (argument == NULL || argument->kind != MYLITE_SQL_AST_LITERAL) {
        return 0U;
    }
    switch (argument->literal_kind) {
    case MYLITE_SQL_AST_LITERAL_NULL:
        return mylite_format_null_character_length;
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        return argument->span.length + mylite_format_literal_extra_length;
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
        return 0U;
    case MYLITE_SQL_AST_LITERAL_FLOAT:
        return mylite_format_float_character_length;
    case MYLITE_SQL_AST_LITERAL_STRING:
    case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
    case MYLITE_SQL_AST_LITERAL_BINARY_STRING:
        return argument->span.length >= 2U
                   ? argument->span.length - 2U + mylite_format_literal_extra_length
                   : mylite_format_literal_extra_length;
    case MYLITE_SQL_AST_LITERAL_TRUE:
    case MYLITE_SQL_AST_LITERAL_FALSE:
    case MYLITE_SQL_AST_LITERAL_DATE:
    case MYLITE_SQL_AST_LITERAL_TIME:
    case MYLITE_SQL_AST_LITERAL_TIMESTAMP:
    case MYLITE_SQL_AST_LITERAL_HEX:
    case MYLITE_SQL_AST_LITERAL_BIT:
    case MYLITE_SQL_AST_LITERAL_NONE:
        break;
    }
    return 0U;
}
