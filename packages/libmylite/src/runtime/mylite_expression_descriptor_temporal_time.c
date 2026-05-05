#include "mylite_expression_descriptor_temporal_time.h"

#include "mylite_expression.h"
#include "mylite_expression_descriptor.h"
#include "mylite_expression_descriptor_temporal.h"
#include "mylite_function_names.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"

#include <string.h>

static unsigned int
time_function_argument_decimals(const struct mylite_sql_ast_node *argument,
                                const struct mylite_field_descriptor *descriptor,
                                const struct mylite_expression_value *value);
static unsigned int
sec_to_time_function_argument_decimals(const struct mylite_sql_ast_node *argument,
                                       const struct mylite_field_descriptor *descriptor);
static bool time_function_argument_is_approximate(const struct mylite_sql_ast_node *argument,
                                                  const struct mylite_field_descriptor *descriptor);
static struct mylite_field_descriptor time_function_descriptor(unsigned int decimals);
static unsigned int time_function_value_decimals(const struct mylite_expression_value *value);

int mylite_expression_descriptor_infer_time_function(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *argument = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor argument_descriptor = mylite_expression_descriptor_defaults();
    struct mylite_expression_value evaluated_value = {0};
    struct mylite_expression_warnings warnings = {0};
    const struct mylite_expression_value *descriptor_value = value;
    unsigned int decimals = 0U;
    int status = MYLITE_OK;

    if (!mylite_function_name_is_time_extraction(name)) {
        return MYLITE_UNSUPPORTED;
    }
    if (descriptor_value == NULL && mylite_expression_is_cacheable_no_table(expression) &&
        mylite_expression_eval(expression, &warnings, &evaluated_value) == 0) {
        descriptor_value = &evaluated_value;
    }
    status = callbacks->infer_expression_descriptor(database, plan, argument, NULL,
                                                    &argument_descriptor);
    if (status != MYLITE_OK) {
        goto cleanup;
    }
    if (argument_descriptor.type == MYLITE_FIELD_TYPE_TIME ||
        argument_descriptor.type == MYLITE_FIELD_TYPE_DATETIME ||
        argument_descriptor.type == MYLITE_FIELD_TYPE_TIMESTAMP) {
        decimals = argument_descriptor.decimals;
    } else if (argument_descriptor.type == MYLITE_FIELD_TYPE_DATE ||
               argument_descriptor.type == MYLITE_FIELD_TYPE_NULL) {
        decimals = 0U;
    } else {
        decimals =
            time_function_argument_decimals(argument, &argument_descriptor, descriptor_value);
    }
    *out_descriptor = time_function_descriptor(decimals);
    status = MYLITE_OK;

cleanup:
    mylite_expression_value_deinit(&evaluated_value);
    mylite_expression_warnings_deinit(&warnings);
    return status;
}

int mylite_expression_descriptor_infer_sec_to_time_function(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *argument = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor argument_descriptor = mylite_expression_descriptor_defaults();
    unsigned int decimals = 0U;
    int status = MYLITE_OK;

    if (!mylite_function_name_is_sec_to_time(name)) {
        return MYLITE_UNSUPPORTED;
    }
    status = callbacks->infer_expression_descriptor(database, plan, argument, NULL,
                                                    &argument_descriptor);
    if (status != MYLITE_OK) {
        return status;
    }
    decimals = sec_to_time_function_argument_decimals(argument, &argument_descriptor);
    *out_descriptor = time_function_descriptor(decimals);
    return MYLITE_OK;
}

static unsigned int
time_function_argument_decimals(const struct mylite_sql_ast_node *argument,
                                const struct mylite_field_descriptor *descriptor,
                                const struct mylite_expression_value *value)
{
    if (time_function_argument_is_approximate(argument, descriptor)) {
        return mylite_mysql_temporal_max_fsp;
    }
    if (descriptor != NULL && descriptor->type == MYLITE_FIELD_TYPE_NEWDECIMAL) {
        return descriptor->decimals > mylite_mysql_temporal_max_fsp ? mylite_mysql_temporal_max_fsp
                                                                    : descriptor->decimals;
    }
    if (mylite_expression_descriptor_has_text_result(descriptor)) {
        if (value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
            return time_function_value_decimals(value);
        }
        if (value == NULL || value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
            return mylite_mysql_temporal_max_fsp;
        }
    }
    return 0U;
}

static unsigned int
sec_to_time_function_argument_decimals(const struct mylite_sql_ast_node *argument,
                                       const struct mylite_field_descriptor *descriptor)
{
    if (time_function_argument_is_approximate(argument, descriptor)) {
        return mylite_mysql_temporal_max_fsp;
    }
    if (descriptor != NULL && descriptor->type == MYLITE_FIELD_TYPE_NEWDECIMAL) {
        return descriptor->decimals > mylite_mysql_temporal_max_fsp ? mylite_mysql_temporal_max_fsp
                                                                    : descriptor->decimals;
    }
    if (mylite_expression_descriptor_has_text_result(descriptor)) {
        return mylite_mysql_temporal_max_fsp;
    }
    return 0U;
}

static bool time_function_argument_is_approximate(const struct mylite_sql_ast_node *argument,
                                                  const struct mylite_field_descriptor *descriptor)
{
    argument = mylite_sql_ast_unwrap_parenthesized_expression(argument);
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (argument->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        argument =
            mylite_sql_ast_unwrap_parenthesized_expression(mylite_ast_child_at(argument, 0U));
    }
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_LITERAL &&
        argument->literal_kind == MYLITE_SQL_AST_LITERAL_FLOAT) {
        return true;
    }
    if (descriptor == NULL) {
        return false;
    }
    if (descriptor->type == MYLITE_FIELD_TYPE_FLOAT) {
        return true;
    }
    return descriptor->type == MYLITE_FIELD_TYPE_DOUBLE;
}

static struct mylite_field_descriptor time_function_descriptor(unsigned int decimals)
{
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_TIME,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = decimals == 0U ? mylite_mysql_time_display_length
                                 : mylite_mysql_time_fraction_display_base + decimals,
        .decimals = decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };

    mylite_field_descriptor_set_nullable(&descriptor, true);
    return descriptor;
}

static unsigned int time_function_value_decimals(const struct mylite_expression_value *value)
{
    const char *dot = value == NULL ? NULL : strchr(value->text_value, '.');
    unsigned int decimals = 0U;

    if (dot == NULL) {
        return 0U;
    }
    for (++dot; *dot >= '0' && *dot <= '9' && decimals < mylite_mysql_temporal_max_fsp; ++dot) {
        ++decimals;
    }
    return decimals;
}
