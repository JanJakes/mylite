#include "mylite_expression_descriptor_compare.h"

#include "mylite_expression_descriptor.h"
#include "mylite_function_names.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

#include <stdint.h>

static int greatest_least_function_uses_string_domain(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    bool *out_string_domain,
    const struct mylite_expression_descriptor_compare_callbacks *callbacks
);

static int infer_greatest_least_string_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_compare_callbacks *callbacks
);

static uint64_t greatest_least_string_result_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    const struct mylite_expression_descriptor_compare_callbacks *callbacks
);

static uint64_t greatest_least_argument_string_length(
    mylite_db *database,
    const struct mylite_field_descriptor *descriptor
);

static int infer_greatest_least_numeric_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_compare_callbacks *callbacks
);

static void aggregate_greatest_least_numeric_descriptor(
    const struct mylite_field_descriptor *argument,
    struct mylite_field_descriptor *aggregate,
    bool *out_saw_nonnull
);

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_expression_descriptor_infer_greatest_least_function(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_compare_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    bool string_domain = false;
    int status = MYLITE_OK;

    if (!mylite_function_name_is_greatest_least(name)) {
        return MYLITE_UNSUPPORTED;
    }
    if (callbacks == NULL || callbacks->infer_expression_descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    status = greatest_least_function_uses_string_domain(
        database,
        plan,
        arguments,
        &string_domain,
        callbacks
    );
    if (status != MYLITE_OK) {
        return status;
    }
    if (string_domain) {
        return infer_greatest_least_string_descriptor(
            database,
            plan,
            arguments,
            result_nullable,
            out_descriptor,
            callbacks
        );
    }
    return infer_greatest_least_numeric_descriptor(
        database,
        plan,
        arguments,
        result_nullable,
        out_descriptor,
        callbacks
    );
}

// NOLINTNEXTLINE(misc-no-recursion)
static int greatest_least_function_uses_string_domain(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    bool *out_string_domain,
    const struct mylite_expression_descriptor_compare_callbacks *callbacks
) {
    *out_string_domain = false;
    for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                     : arguments->first_child;
         child != NULL;
         child = child->next_sibling) {
        struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
        int status =
            callbacks->infer_expression_descriptor(database, plan, child, NULL, &descriptor);

        if (status != MYLITE_OK) {
            return status;
        }
        if (mylite_expression_descriptor_has_text_result(&descriptor) ||
            descriptor.type == MYLITE_FIELD_TYPE_BLOB) {
            *out_string_domain = true;
            return MYLITE_OK;
        }
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_greatest_least_string_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_compare_callbacks *callbacks
) {
    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = 0U,
        .length = greatest_least_string_result_length(database, plan, arguments, callbacks),
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_charset_id(database),
        .nullable = result_nullable,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, result_nullable);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t greatest_least_string_result_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    const struct mylite_expression_descriptor_compare_callbacks *callbacks
) {
    uint64_t length = 0U;

    for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                     : arguments->first_child;
         child != NULL;
         child = child->next_sibling) {
        struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();

        if (callbacks->infer_expression_descriptor(database, plan, child, NULL, &descriptor) !=
            MYLITE_OK) {
            return mylite_mysql_long_text_length;
        }
        length = mylite_expression_descriptor_max_u64(
            length,
            greatest_least_argument_string_length(database, &descriptor)
        );
    }
    return length;
}

static uint64_t greatest_least_argument_string_length(
    mylite_db *database,
    const struct mylite_field_descriptor *descriptor
) {
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);

    if (descriptor == NULL || descriptor->type == MYLITE_FIELD_TYPE_NULL) {
        return 0U;
    }
    if (mylite_expression_descriptor_has_text_result(descriptor) ||
        descriptor->type == MYLITE_FIELD_TYPE_BLOB) {
        return descriptor->length;
    }
    if (max_bytes_per_character != 0U &&
        descriptor->length > UINT64_MAX / max_bytes_per_character) {
        return mylite_mysql_long_text_length;
    }
    return descriptor->length * max_bytes_per_character;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_greatest_least_numeric_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_compare_callbacks *callbacks
) {
    struct mylite_field_descriptor aggregate = mylite_expression_descriptor_defaults();
    bool saw_nonnull = false;

    for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                     : arguments->first_child;
         child != NULL;
         child = child->next_sibling) {
        struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
        int status =
            callbacks->infer_expression_descriptor(database, plan, child, NULL, &descriptor);

        if (status != MYLITE_OK) {
            return status;
        }
        aggregate_greatest_least_numeric_descriptor(&descriptor, &aggregate, &saw_nonnull);
    }

    if (!saw_nonnull) {
        *out_descriptor = mylite_expression_descriptor_null();
    } else {
        *out_descriptor = aggregate;
    }
    mylite_field_descriptor_set_nullable(out_descriptor, result_nullable);
    return MYLITE_OK;
}

static void aggregate_greatest_least_numeric_descriptor(
    const struct mylite_field_descriptor *argument,
    struct mylite_field_descriptor *aggregate,
    bool *out_saw_nonnull
) {
    if (argument == NULL || argument->type == MYLITE_FIELD_TYPE_NULL) {
        return;
    }

    if (!*out_saw_nonnull) {
        *aggregate = *argument;
        aggregate->flags |= MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
        aggregate->charset_id = mylite_mysql_binary_charset_id;
        *out_saw_nonnull = true;
        return;
    }

    if (mylite_expression_descriptor_has_double_result(argument) ||
        argument->type == MYLITE_FIELD_TYPE_FLOAT ||
        mylite_expression_descriptor_has_double_result(aggregate) ||
        aggregate->type == MYLITE_FIELD_TYPE_FLOAT) {
        *aggregate = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_DOUBLE,
            .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_expression_descriptor_max_u64(
                mylite_expression_descriptor_max_u64(aggregate->length, argument->length),
                mylite_mysql_double_display_length
            ),
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = true,
        };
        return;
    }

    if (mylite_expression_descriptor_has_decimal_result(argument) ||
        mylite_expression_descriptor_has_decimal_result(aggregate)) {
        *aggregate = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_NEWDECIMAL,
            .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_expression_descriptor_max_u64(aggregate->length, argument->length),
            .decimals =
                aggregate->decimals > argument->decimals ? aggregate->decimals : argument->decimals,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = true,
        };
        return;
    }

    aggregate->type = MYLITE_FIELD_TYPE_LONGLONG;
    aggregate->flags |= MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
    if (((aggregate->flags & MYLITE_FIELD_FLAG_UNSIGNED) == 0U) ||
        ((argument->flags & MYLITE_FIELD_FLAG_UNSIGNED) == 0U)) {
        aggregate->flags &= ~(unsigned int)MYLITE_FIELD_FLAG_UNSIGNED;
    }
    aggregate->charset_id = mylite_mysql_binary_charset_id;
    aggregate->length = mylite_expression_descriptor_max_u64(aggregate->length, argument->length);
    aggregate->decimals = 0U;
}
