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
    case MYLITE_SQL_AST_LITERAL_FLOAT:
        descriptor = mylite_expression_descriptor_decimal(false);
        descriptor.decimals = mylite_expression_descriptor_literal_decimal_scale(expression);
        descriptor.length = expression->span.length + 1U;
        break;
    case MYLITE_SQL_AST_LITERAL_STRING:
    case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
        descriptor.type = MYLITE_FIELD_TYPE_VAR_STRING;
        descriptor.length = mylite_expression_descriptor_string_length(database, value, expression);
        descriptor.decimals = mylite_mysql_not_fixed_decimals;
        descriptor.charset_id = mylite_expression_descriptor_connection_charset_id(database);
        mylite_field_descriptor_set_not_null(&descriptor, true);
        break;
    case MYLITE_SQL_AST_LITERAL_HEX:
    case MYLITE_SQL_AST_LITERAL_BIT:
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
