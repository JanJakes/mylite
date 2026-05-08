#include "mylite_expression_descriptor_basic.h"

#include "mylite_expression_descriptor.h"
#include "mylite_metadata_constants.h"
#include "mylite_select.h"
#include "mylite_select_resolve.h"
#include "mylite_span.h"
#include "mylite_system_variables.h"
#include "mylite_user_variables.h"
#include "sql/mylite_ast.h"

#include <stdbool.h>
#include <stddef.h>

static struct mylite_field_descriptor temporal_literal_descriptor(
    const struct mylite_sql_ast_node *expression,
    enum mylite_field_type type
);

static struct mylite_field_descriptor hex_literal_descriptor(
    const struct mylite_sql_ast_node *expression
);

static struct mylite_field_descriptor bit_literal_descriptor(
    const struct mylite_sql_ast_node *expression
);

static struct mylite_field_descriptor binary_string_literal_descriptor(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value
);

static uint64_t decimal_literal_descriptor_length(const struct mylite_sql_ast_node *expression);

static struct mylite_field_descriptor float_literal_descriptor(
    const struct mylite_sql_ast_node *expression
);

static uint64_t hex_literal_byte_length(const struct mylite_sql_ast_node *expression);

static uint64_t bit_literal_byte_length(const struct mylite_sql_ast_node *expression);

static uint64_t binary_string_literal_byte_length(const struct mylite_sql_ast_node *expression);

static size_t literal_digit_count(const struct mylite_sql_ast_node *expression);

static unsigned int temporal_literal_fraction_digits(const struct mylite_sql_ast_node *expression);

int mylite_expression_descriptor_infer_literal(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor
) {
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();

    switch (expression->literal_kind) {
    case MYLITE_SQL_AST_LITERAL_NULL:
        descriptor.flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
        break;
    case MYLITE_SQL_AST_LITERAL_TRUE:
    case MYLITE_SQL_AST_LITERAL_FALSE:
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        descriptor = mylite_expression_descriptor_signed_longlong(false);
        descriptor.length = mylite_expression_descriptor_literal_integer_length(expression, value);
        if (value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
            descriptor.flags |= MYLITE_FIELD_FLAG_UNSIGNED;
        }
        break;
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
        descriptor = mylite_expression_descriptor_decimal(false);
        descriptor.decimals = mylite_expression_descriptor_literal_decimal_scale(expression);
        descriptor.length = decimal_literal_descriptor_length(expression);
        break;
    case MYLITE_SQL_AST_LITERAL_FLOAT:
        descriptor = float_literal_descriptor(expression);
        break;
    case MYLITE_SQL_AST_LITERAL_STRING:
    case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
        descriptor.type = MYLITE_FIELD_TYPE_VAR_STRING;
        descriptor.length = mylite_expression_descriptor_string_length(database, value, expression);
        descriptor.decimals = mylite_mysql_not_fixed_decimals;
        descriptor.charset_id = mylite_expression_descriptor_connection_charset_id(database);
        mylite_field_descriptor_set_not_null(&descriptor, true);
        break;
    case MYLITE_SQL_AST_LITERAL_BINARY_STRING:
        descriptor = binary_string_literal_descriptor(expression, value);
        break;
    case MYLITE_SQL_AST_LITERAL_HEX:
        descriptor = hex_literal_descriptor(expression);
        break;
    case MYLITE_SQL_AST_LITERAL_BIT:
        descriptor = bit_literal_descriptor(expression);
        break;
    case MYLITE_SQL_AST_LITERAL_NONE:
        descriptor = mylite_expression_descriptor_from_value(value);
        break;
    case MYLITE_SQL_AST_LITERAL_DATE:
        descriptor = temporal_literal_descriptor(expression, MYLITE_FIELD_TYPE_DATE);
        break;
    case MYLITE_SQL_AST_LITERAL_TIME:
        descriptor = temporal_literal_descriptor(expression, MYLITE_FIELD_TYPE_TIME);
        break;
    case MYLITE_SQL_AST_LITERAL_TIMESTAMP:
        descriptor = temporal_literal_descriptor(expression, MYLITE_FIELD_TYPE_DATETIME);
        break;
    }

    *out_descriptor = descriptor;
    return MYLITE_OK;
}

static uint64_t decimal_literal_descriptor_length(const struct mylite_sql_ast_node *expression) {
    const char *text = expression == NULL ? NULL : expression->span.text;
    size_t text_length = expression == NULL ? 0U : expression->span.length;
    bool negative = false;
    size_t sign_offset = 0U;
    size_t integer_start = 0U;
    size_t integer_end = 0U;
    size_t first_nonzero = 0U;
    size_t fraction_length = 0U;
    size_t integer_length = 0U;
    uint64_t output_length = 0U;

    if (text == NULL || text_length == 0U) {
        return 0U;
    }
    negative = text[0] == '-';
    sign_offset = text[0] == '-' || text[0] == '+' ? 1U : 0U;
    integer_start = sign_offset;
    integer_end = integer_start;
    first_nonzero = integer_start;
    output_length = negative ? 1U : 0U;
    while (integer_end < text_length && text[integer_end] != '.') {
        ++integer_end;
    }
    fraction_length = integer_end < text_length ? text_length - integer_end - 1U : 0U;
    while (first_nonzero < integer_end && text[first_nonzero] == '0') {
        ++first_nonzero;
    }
    integer_length = first_nonzero == integer_end ? 1U : integer_end - first_nonzero;
    output_length += integer_length + (fraction_length == 0U ? 0U : 1U + fraction_length);
    if (integer_end == integer_start) {
        return output_length;
    }
    return output_length + (first_nonzero > integer_start && first_nonzero < integer_end ? 2U : 1U);
}

static struct mylite_field_descriptor float_literal_descriptor(
    const struct mylite_sql_ast_node *expression
) {
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_DOUBLE,
        .flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = expression == NULL ? 0U : expression->span.length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = false,
    };
}

static struct mylite_field_descriptor hex_literal_descriptor(
    const struct mylite_sql_ast_node *expression
) {
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_BINARY,
        .length = hex_literal_byte_length(expression),
        .decimals = 0U,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = false,
    };
}

static struct mylite_field_descriptor bit_literal_descriptor(
    const struct mylite_sql_ast_node *expression
) {
    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY,
        .length = bit_literal_byte_length(expression),
        .decimals = 0U,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = false,
    };
}

static struct mylite_field_descriptor binary_string_literal_descriptor(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value
) {
    uint64_t length = value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_TEXT
                          ? (uint64_t)value->text_length
                          : binary_string_literal_byte_length(expression);

    return (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = false,
    };
}

static uint64_t hex_literal_byte_length(const struct mylite_sql_ast_node *expression) {
    size_t digits = literal_digit_count(expression);

    return (uint64_t)((digits + 1U) / 2U);
}

static uint64_t bit_literal_byte_length(const struct mylite_sql_ast_node *expression) {
    size_t digits = literal_digit_count(expression);

    return (uint64_t)((digits + 7U) / 8U);
}

static uint64_t binary_string_literal_byte_length(const struct mylite_sql_ast_node *expression) {
    const char *text = expression == NULL ? NULL : expression->span.text;
    size_t length = expression == NULL ? 0U : expression->span.length;
    size_t start = 0U;
    size_t end = length;
    char quote = '\0';
    uint64_t output = 0U;

    if (text == NULL) {
        return 0U;
    }
    for (size_t index = 0U; index < length; ++index) {
        if (text[index] == '\'' || text[index] == '"') {
            quote = text[index];
            start = index + 1U;
            end = length > 0U && text[length - 1U] == quote ? length - 1U : length;
            break;
        }
    }
    for (size_t index = start; index < end; ++index) {
        if (!expression->no_backslash_escapes && text[index] == '\\' && index + 1U < end) {
            switch (text[index + 1U]) {
            case '\'':
            case '"':
            case '\\':
            case '0':
            case 'b':
            case 'n':
            case 'r':
            case 't':
            case 'Z':
                ++index;
                break;
            default:
                break;
            }
        } else if (text[index] == quote && index + 1U < end && text[index + 1U] == quote) {
            ++index;
        }
        ++output;
    }
    return output;
}

static size_t literal_digit_count(const struct mylite_sql_ast_node *expression) {
    const char *text = expression == NULL ? NULL : expression->span.text;
    size_t length = expression == NULL ? 0U : expression->span.length;

    if (text == NULL) {
        return 0U;
    }
    if (length >= 3U && text[1] == '\'' && text[length - 1U] == '\'') {
        return length - 3U;
    }
    return length < 2U ? 0U : length - 2U;
}

static struct mylite_field_descriptor temporal_literal_descriptor(
    const struct mylite_sql_ast_node *expression,
    enum mylite_field_type type
) {
    unsigned int decimals = temporal_literal_fraction_digits(expression);
    struct mylite_field_descriptor descriptor = {
        .type = type,
        .flags = MYLITE_FIELD_FLAG_NOT_NULL | MYLITE_FIELD_FLAG_BINARY,
        .decimals = decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = false,
    };

    if (type == MYLITE_FIELD_TYPE_DATE) {
        descriptor.length = mylite_mysql_date_display_length;
    } else if (type == MYLITE_FIELD_TYPE_TIME) {
        descriptor.length = decimals == 0U ? mylite_mysql_time_display_length
                                           : mylite_mysql_time_fraction_display_base + decimals;
    } else {
        descriptor.length = decimals == 0U ? mylite_mysql_datetime_display_length
                                           : mylite_mysql_datetime_fraction_display_base + decimals;
    }
    return descriptor;
}

static unsigned int temporal_literal_fraction_digits(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *value = mylite_ast_child_at(expression, 0U);
    const char *text = value == NULL ? NULL : value->span.text;
    size_t length = value == NULL ? 0U : value->span.length;
    bool in_fraction = false;
    unsigned int digits = 0U;

    for (size_t index = 0U; index < length; ++index) {
        if (text[index] == '.') {
            in_fraction = true;
            continue;
        }
        if (in_fraction && text[index] >= '0' && text[index] <= '9') {
            if (digits < mylite_mysql_temporal_max_fsp) {
                ++digits;
            }
            continue;
        }
        if (in_fraction) {
            break;
        }
    }
    return digits;
}

int mylite_expression_descriptor_infer_identifier(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor
) {
    if (mylite_system_variable_identifier_is_system_variable(expression)) {
        return mylite_system_variable_infer_identifier(database, expression, out_descriptor);
    }
    if (mylite_user_variable_identifier_is_user_variable(expression)) {
        return mylite_user_variable_infer_identifier(database, expression, out_descriptor);
    }

    size_t column_index = plan == NULL ? 0U : mylite_select_plan_column_count(plan);
    int status = plan == NULL ? MYLITE_UNSUPPORTED
                              : mylite_select_resolve_plan_column_reference(
                                    database,
                                    plan,
                                    expression,
                                    "field list",
                                    &column_index
                                );

    if (status != MYLITE_OK || column_index >= mylite_select_plan_column_count(plan)) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status == MYLITE_OK ? MYLITE_UNSUPPORTED : status;
    }

    const struct mylite_select_column *column =
        mylite_select_plan_column_const(plan, column_index, NULL);

    if (column == NULL) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_UNSUPPORTED;
    }
    *out_descriptor = column->descriptor;
    return MYLITE_OK;
}
