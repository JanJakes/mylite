#include "mylite_expression_descriptor_aggregate.h"

#include "mylite_connection.h"
#include "mylite_expression_descriptor.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

#include <stdbool.h>

static int infer_group_concat_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_aggregate_callbacks *callbacks
);

static int group_concat_argument_list_has_binary_result(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    const struct mylite_expression_descriptor_aggregate_callbacks *callbacks,
    bool *out_has_binary_result
);

static uint64_t group_concat_text_descriptor_length(
    uint64_t group_concat_max_len,
    uint64_t character_max_length
);

static bool descriptor_is_binary_string(const struct mylite_field_descriptor *descriptor);

int mylite_expression_descriptor_infer_aggregate_expression(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_aggregate_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *argument = mylite_ast_child_at(expression, 1U);
    struct mylite_field_descriptor argument_descriptor = mylite_expression_descriptor_defaults();
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();

    if (callbacks == NULL || callbacks->infer_expression_descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    if (expression->aggregate_argument == MYLITE_SQL_AST_AGGREGATE_ARGUMENT_EXPRESSION) {
        int status =
            callbacks
                ->infer_expression_descriptor(database, plan, argument, NULL, &argument_descriptor);

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
        } else if (
            (argument_descriptor.flags & MYLITE_FIELD_FLAG_NUM) != 0U &&
            argument_descriptor.type != MYLITE_FIELD_TYPE_FLOAT &&
            argument_descriptor.type != MYLITE_FIELD_TYPE_DOUBLE
        ) {
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
    case MYLITE_SQL_AST_AGGREGATE_GROUP_CONCAT: {
        return infer_group_concat_descriptor(database, plan, expression, out_descriptor, callbacks);
    }
    case MYLITE_SQL_AST_AGGREGATE_NONE:
        break;
    }

    *out_descriptor = mylite_expression_descriptor_defaults();
    return MYLITE_OK;
}

static int infer_group_concat_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_aggregate_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    uint64_t group_concat_max_len = mylite_connection_group_concat_max_len(database);
    uint64_t character_max_length =
        mylite_expression_descriptor_connection_character_max_length(database);
    bool has_binary_result = false;
    int status = group_concat_argument_list_has_binary_result(
        database,
        plan,
        arguments,
        callbacks,
        &has_binary_result
    );
    struct mylite_field_descriptor descriptor = {0};

    if (status != MYLITE_OK) {
        return status;
    }
    if (has_binary_result) {
        descriptor = (struct mylite_field_descriptor){
            .type = group_concat_max_len <= 512U ? MYLITE_FIELD_TYPE_VAR_STRING
                                                 : MYLITE_FIELD_TYPE_BLOB,
            .flags = MYLITE_FIELD_FLAG_BINARY,
            .length = group_concat_max_len,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = true,
        };
    } else {
        descriptor = (struct mylite_field_descriptor){
            .type = group_concat_max_len <= 512U ? MYLITE_FIELD_TYPE_VAR_STRING
                                                 : MYLITE_FIELD_TYPE_BLOB,
            .flags = 0U,
            .length =
                group_concat_text_descriptor_length(group_concat_max_len, character_max_length),
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_collation_id(database),
            .nullable = true,
        };
    }
    mylite_field_descriptor_set_nullable(&descriptor, true);
    *out_descriptor = descriptor;
    return MYLITE_OK;
}

static uint64_t group_concat_text_descriptor_length(
    uint64_t group_concat_max_len,
    uint64_t character_max_length
) {
    enum { mysql_group_concat_long_text_length_factor = 16U };

    uint64_t multiplier = character_max_length;

    if (multiplier == 0U) {
        multiplier = 1U;
    }
    if (group_concat_max_len > 512U) {
        if (multiplier > UINT64_MAX / mysql_group_concat_long_text_length_factor) {
            return UINT64_MAX;
        }
        multiplier *= mysql_group_concat_long_text_length_factor;
    }
    return group_concat_max_len > UINT64_MAX / multiplier ? UINT64_MAX
                                                          : group_concat_max_len * multiplier;
}

static int group_concat_argument_list_has_binary_result(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    const struct mylite_expression_descriptor_aggregate_callbacks *callbacks,
    bool *out_has_binary_result
) {
    *out_has_binary_result = false;
    for (const struct mylite_sql_ast_node *argument = arguments == NULL ? NULL
                                                                        : arguments->first_child;
         argument != NULL;
         argument = argument->next_sibling) {
        struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
        int status =
            callbacks->infer_expression_descriptor(database, plan, argument, NULL, &descriptor);

        if (status != MYLITE_OK) {
            return status;
        }
        if (descriptor_is_binary_string(&descriptor)) {
            *out_has_binary_result = true;
            return MYLITE_OK;
        }
    }
    return MYLITE_OK;
}

static bool descriptor_is_binary_string(const struct mylite_field_descriptor *descriptor) {
    if (descriptor == NULL || descriptor->type == MYLITE_FIELD_TYPE_NULL) {
        return false;
    }
    if ((descriptor->flags & MYLITE_FIELD_FLAG_NUM) != 0U) {
        return false;
    }
    return descriptor->charset_id == mylite_mysql_binary_charset_id;
}
