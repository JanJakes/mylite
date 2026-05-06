#include "mylite_expression_descriptor_aggregate.h"

#include "mylite_expression_descriptor.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

int mylite_expression_descriptor_infer_aggregate_expression(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_aggregate_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *argument = mylite_ast_child_at(expression, 1U);
    struct mylite_field_descriptor argument_descriptor = mylite_expression_descriptor_defaults();
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();

    if (callbacks == NULL || callbacks->infer_expression_descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    if (expression->aggregate_argument == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION) {
        int status = callbacks->infer_expression_descriptor(database, plan, argument, NULL,
                                                            &argument_descriptor);

        if (status != MYLITE_OK) {
            return status;
        }
    }

    switch (expression->aggregate_kind) {
    case MYLITE_SQL_AST_AGGREGATE_COUNT:
        descriptor = mylite_expression_descriptor_signed_longlong(false);
        descriptor.length = mylite_mysql_signed_longlong_display_length;
        mylite_field_descriptor_set_not_null(&descriptor, true);
        *out_descriptor = descriptor;
        return MYLITE_OK;
    case MYLITE_SQL_AST_AGGREGATE_SUM:
        if (argument_descriptor.type == MYLITE_FIELD_TYPE_NEWDECIMAL) {
            descriptor = mylite_expression_descriptor_decimal(true);
            descriptor.length = mylite_mysql_sum_decimal_display_length;
            descriptor.decimals = argument_descriptor.decimals;
        } else if ((argument_descriptor.flags & MYLITE_FIELD_FLAG_NUM) != 0U &&
                   argument_descriptor.type != MYLITE_FIELD_TYPE_FLOAT &&
                   argument_descriptor.type != MYLITE_FIELD_TYPE_DOUBLE) {
            descriptor = mylite_expression_descriptor_decimal(true);
            descriptor.length = mylite_mysql_sum_integer_display_length;
            descriptor.decimals = 0U;
        } else {
            descriptor = (struct mylite_field_descriptor){
                .type = MYLITE_FIELD_TYPE_DOUBLE,
                .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
                .length = mylite_mysql_double_display_length,
                .decimals = mylite_mysql_not_fixed_decimals,
                .charset_id = mylite_mysql_binary_charset_id,
                .nullable = true,
            };
            mylite_field_descriptor_set_nullable(&descriptor, true);
        }
        *out_descriptor = descriptor;
        return MYLITE_OK;
    case MYLITE_SQL_AST_AGGREGATE_AVG:
        if (argument_descriptor.type == MYLITE_FIELD_TYPE_FLOAT ||
            argument_descriptor.type == MYLITE_FIELD_TYPE_DOUBLE ||
            (argument_descriptor.flags & MYLITE_FIELD_FLAG_NUM) == 0U) {
            descriptor = (struct mylite_field_descriptor){
                .type = MYLITE_FIELD_TYPE_DOUBLE,
                .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
                .length = mylite_mysql_double_display_length,
                .decimals = mylite_mysql_not_fixed_decimals,
                .charset_id = mylite_mysql_binary_charset_id,
                .nullable = true,
            };
            mylite_field_descriptor_set_nullable(&descriptor, true);
        } else {
            descriptor = mylite_expression_descriptor_decimal(true);
            descriptor.length = mylite_mysql_avg_display_length;
            descriptor.decimals = argument_descriptor.type == MYLITE_FIELD_TYPE_NEWDECIMAL
                                      ? mylite_mysql_avg_decimal_scale
                                      : mylite_mysql_avg_integer_scale;
        }
        *out_descriptor = descriptor;
        return MYLITE_OK;
    case MYLITE_SQL_AST_AGGREGATE_MIN:
    case MYLITE_SQL_AST_AGGREGATE_MAX:
        descriptor = argument_descriptor;
        mylite_field_descriptor_set_nullable(&descriptor, true);
        *out_descriptor = descriptor;
        return MYLITE_OK;
    case MYLITE_SQL_AST_AGGREGATE_GROUP_CONCAT:
        descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_BLOB,
            .flags = MYLITE_FIELD_FLAG_BLOB,
            .length =
                1024U * mylite_expression_descriptor_connection_character_max_length(database),
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_collation_id(database),
            .nullable = true,
        };
        mylite_field_descriptor_set_nullable(&descriptor, true);
        *out_descriptor = descriptor;
        return MYLITE_OK;
    case MYLITE_SQL_AST_AGGREGATE_NONE:
        break;
    }

    *out_descriptor = mylite_expression_descriptor_defaults();
    return MYLITE_OK;
}
