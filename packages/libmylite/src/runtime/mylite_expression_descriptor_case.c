#include "mylite_expression_descriptor_case.h"

#include "mylite_expression_descriptor.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

struct mylite_case_descriptor_aggregate {
    struct mylite_field_descriptor descriptor;
    bool has_result;
    bool has_non_null_result;
    bool has_text_result;
    bool has_binary_text_result;
    bool has_decimal_result;
    bool has_double_result;
    bool nullable;
};

static int infer_case_result_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_case_descriptor_aggregate *aggregate,
    const struct mylite_expression_descriptor_case_callbacks *callbacks
);

static void aggregate_case_result_descriptor(
    const struct mylite_field_descriptor *descriptor,
    struct mylite_case_descriptor_aggregate *aggregate
);

static bool case_result_descriptor_uses_binary_text(
    const struct mylite_field_descriptor *descriptor
);

static struct mylite_field_descriptor finalize_case_descriptor(
    mylite_db *database,
    const struct mylite_case_descriptor_aggregate *aggregate
);

int mylite_expression_descriptor_infer_case_expression(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_case_callbacks *callbacks
) {
    size_t when_list_index = 0U;
    size_t else_expression_index = 1U;
    const struct mylite_sql_ast_node *when_list = NULL;
    const struct mylite_sql_ast_node *else_expression = NULL;
    struct mylite_case_descriptor_aggregate aggregate = {0};

    if (callbacks == NULL || callbacks->infer_expression_descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    if (expression->case_expression_simple) {
        when_list_index = 1U;
        else_expression_index = 2U;
    }
    when_list = mylite_ast_child_at(expression, when_list_index);
    else_expression = mylite_ast_child_at(expression, else_expression_index);

    aggregate.descriptor = mylite_expression_descriptor_defaults();
    if (when_list == NULL || when_list->kind != MYLITE_SQL_AST_CASE_WHEN_LIST) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_OK;
    }

    for (const struct mylite_sql_ast_node *arm = when_list->first_child; arm != NULL;
         arm = arm->next_sibling) {
        int status = infer_case_result_descriptor(
            database,
            plan,
            mylite_ast_child_at(arm, 1U),
            &aggregate,
            callbacks
        );

        if (status != MYLITE_OK) {
            return status;
        }
    }
    if (else_expression == NULL) {
        aggregate.has_result = true;
        aggregate.nullable = true;
    } else {
        int status =
            infer_case_result_descriptor(database, plan, else_expression, &aggregate, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }

    *out_descriptor = finalize_case_descriptor(database, &aggregate);
    return MYLITE_OK;
}

static int infer_case_result_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_case_descriptor_aggregate *aggregate,
    const struct mylite_expression_descriptor_case_callbacks *callbacks
) {
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
    int status =
        callbacks->infer_expression_descriptor(database, plan, expression, NULL, &descriptor);

    if (status != MYLITE_OK) {
        return status;
    }
    aggregate_case_result_descriptor(&descriptor, aggregate);
    return MYLITE_OK;
}

static void aggregate_case_result_descriptor(
    const struct mylite_field_descriptor *descriptor,
    struct mylite_case_descriptor_aggregate *aggregate
) {
    aggregate->has_result = true;
    if (mylite_expression_descriptor_is_nullable(descriptor)) {
        aggregate->nullable = true;
    }
    if (descriptor->type == MYLITE_FIELD_TYPE_NULL) {
        aggregate->nullable = true;
        return;
    }

    if (!aggregate->has_non_null_result) {
        aggregate->descriptor = *descriptor;
        aggregate->has_non_null_result = true;
    } else {
        aggregate->descriptor.length =
            mylite_expression_descriptor_max_u64(aggregate->descriptor.length, descriptor->length);
        aggregate->descriptor.decimals = (unsigned int)mylite_expression_descriptor_max_u64(
            aggregate->descriptor.decimals,
            descriptor->decimals
        );
        aggregate->descriptor.flags |= descriptor->flags & MYLITE_FIELD_FLAG_UNSIGNED;
        aggregate->descriptor.flags &= ~(unsigned int)MYLITE_FIELD_FLAG_NOT_NULL;
    }

    if (mylite_expression_descriptor_has_text_result(descriptor)) {
        aggregate->has_text_result = true;
        if (case_result_descriptor_uses_binary_text(descriptor)) {
            aggregate->has_binary_text_result = true;
        }
    }
    if (mylite_expression_descriptor_has_decimal_result(descriptor)) {
        aggregate->has_decimal_result = true;
    }
    if (mylite_expression_descriptor_has_double_result(descriptor)) {
        aggregate->has_double_result = true;
    }
}

static bool case_result_descriptor_uses_binary_text(
    const struct mylite_field_descriptor *descriptor
) {
    if (!mylite_expression_descriptor_has_text_result(descriptor)) {
        return false;
    }
    return descriptor->charset_id == mylite_mysql_binary_charset_id;
}

static struct mylite_field_descriptor finalize_case_descriptor(
    mylite_db *database,
    const struct mylite_case_descriptor_aggregate *aggregate
) {
    struct mylite_field_descriptor descriptor = aggregate->descriptor;

    if (!aggregate->has_result || !aggregate->has_non_null_result) {
        return mylite_expression_descriptor_null();
    }
    if (aggregate->has_text_result) {
        unsigned int flags = aggregate->has_binary_text_result ? MYLITE_FIELD_FLAG_BINARY : 0U;
        unsigned int charset_id =
            aggregate->has_binary_text_result
                ? mylite_mysql_binary_charset_id
                : mylite_expression_descriptor_connection_charset_id(database);

        descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .flags = flags,
            .length = aggregate->descriptor.length,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = charset_id,
            .nullable = aggregate->nullable,
        };
    } else if (aggregate->has_decimal_result) {
        descriptor = mylite_expression_descriptor_decimal(aggregate->nullable);
        descriptor.length = aggregate->descriptor.length;
        descriptor.decimals = aggregate->descriptor.decimals;
    } else if (aggregate->has_double_result) {
        descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_DOUBLE,
            .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_expression_descriptor_max_u64(
                aggregate->descriptor.length,
                mylite_mysql_double_display_length
            ),
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = aggregate->nullable,
        };
    } else {
        descriptor.flags |= MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
        descriptor.charset_id = mylite_mysql_binary_charset_id;
    }
    mylite_field_descriptor_set_nullable(&descriptor, aggregate->nullable);
    return descriptor;
}
