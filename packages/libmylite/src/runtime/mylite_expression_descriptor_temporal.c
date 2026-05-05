#include "mylite_expression_descriptor_temporal.h"

#include "mylite_expression.h"
#include "mylite_expression_descriptor.h"
#include "mylite_expression_descriptor_temporal_time.h"
#include "mylite_function_names.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"
#include "mylite_temporal_functions.h"

#include <stdint.h>

static struct mylite_field_descriptor current_date_function_descriptor(void);
static struct mylite_field_descriptor current_time_function_descriptor(unsigned int fsp);
static int infer_from_unixtime_function_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks);
static struct mylite_field_descriptor
from_unixtime_string_descriptor(mylite_db *database, const struct mylite_expression_value *value);
static struct mylite_field_descriptor from_unixtime_datetime_descriptor(unsigned int decimals);
static unsigned int
from_unixtime_argument_decimals(const struct mylite_sql_ast_node *argument,
                                const struct mylite_field_descriptor *descriptor);
static bool from_unixtime_argument_is_approximate(const struct mylite_sql_ast_node *argument,
                                                  const struct mylite_field_descriptor *descriptor);
static int infer_date_interval_function_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks);
static struct mylite_field_descriptor date_interval_string_descriptor(mylite_db *database);
static struct mylite_field_descriptor date_interval_datetime_descriptor(unsigned int decimals);
static bool extract_interval_unit_supported(enum mylite_sql_ast_interval_unit unit);

struct mylite_field_descriptor
mylite_expression_descriptor_current_datetime_function(unsigned int fsp)
{
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_DATETIME,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = fsp == 0U ? mylite_mysql_datetime_display_length
                            : mylite_mysql_datetime_fraction_display_base + fsp,
        .decimals = fsp,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = false,
    };

    mylite_field_descriptor_set_nullable(&descriptor, false);
    return descriptor;
}

bool mylite_expression_descriptor_infer_current_temporal_function(
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    unsigned int fsp = 0U;

    if (!mylite_temporal_current_function_fsp(expression, &fsp)) {
        return false;
    }
    if (mylite_temporal_function_name_is_current_datetime(name)) {
        *out_descriptor = mylite_expression_descriptor_current_datetime_function(fsp);
        return true;
    }
    if (mylite_temporal_function_name_is_current_date(name)) {
        *out_descriptor = current_date_function_descriptor();
        return true;
    }
    if (mylite_temporal_function_name_is_current_time(name)) {
        *out_descriptor = current_time_function_descriptor(fsp);
        return true;
    }
    return false;
}

int mylite_expression_descriptor_infer_temporal_function(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks)
{
    int status = MYLITE_OK;

    if (callbacks == NULL || callbacks->infer_expression_descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    if (mylite_expression_descriptor_infer_current_temporal_function(expression, out_descriptor)) {
        return MYLITE_OK;
    }
    if (mylite_expression_descriptor_infer_temporal_part_function(expression, out_descriptor)) {
        return MYLITE_OK;
    }
    status = infer_from_unixtime_function_descriptor(database, plan, expression, value,
                                                     out_descriptor, callbacks);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = mylite_expression_descriptor_infer_time_function(database, plan, expression, value,
                                                              out_descriptor, callbacks);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    return infer_date_interval_function_descriptor(database, plan, expression, out_descriptor,
                                                   callbacks);
}

bool mylite_expression_descriptor_infer_temporal_scalar_function(
    const struct mylite_sql_ast_node *name, bool arguments_nullable,
    struct mylite_field_descriptor *out_descriptor)
{
    if (mylite_function_name_is_date_extraction(name)) {
        struct mylite_field_descriptor descriptor = {
            .type = MYLITE_FIELD_TYPE_DATE,
            .flags = MYLITE_FIELD_FLAG_BINARY,
            .length = mylite_mysql_date_display_length,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = true,
        };

        mylite_field_descriptor_set_nullable(&descriptor, true);
        *out_descriptor = descriptor;
        return true;
    }
    if (mylite_function_name_is_datediff(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = mylite_mysql_datediff_function_display_length;
        return true;
    }
    if (mylite_function_name_is_timestampdiff(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = mylite_mysql_signed_longlong_display_length;
        return true;
    }
    if (mylite_function_name_is_to_days(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = mylite_mysql_to_days_function_display_length;
        return true;
    }
    if (mylite_function_name_is_to_seconds(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = mylite_mysql_to_seconds_function_display_length;
        return true;
    }
    if (mylite_function_name_is_from_days(name)) {
        struct mylite_field_descriptor descriptor = {
            .type = MYLITE_FIELD_TYPE_DATE,
            .flags = MYLITE_FIELD_FLAG_BINARY,
            .length = mylite_mysql_date_display_length,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = arguments_nullable,
        };

        mylite_field_descriptor_set_nullable(&descriptor, arguments_nullable);
        *out_descriptor = descriptor;
        return true;
    }
    if (mylite_function_name_is_year_part(name)) {
        struct mylite_field_descriptor descriptor = {
            .type = MYLITE_FIELD_TYPE_YEAR,
            .flags = MYLITE_FIELD_FLAG_UNSIGNED | MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_mysql_year_display_length,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = true,
        };

        mylite_field_descriptor_set_nullable(&descriptor, true);
        *out_descriptor = descriptor;
        return true;
    }
    if (mylite_function_name_is_month_part(name) || mylite_function_name_is_day_part(name) ||
        mylite_function_name_is_minute_part(name) || mylite_function_name_is_second_part(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = mylite_mysql_temporal_part_short_display_length;
        return true;
    }
    if (mylite_function_name_is_hour_part(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = mylite_mysql_temporal_part_hour_display_length;
        return true;
    }
    return false;
}

bool mylite_expression_descriptor_infer_temporal_part_function(
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);

    if (mylite_function_name_is_extract(name)) {
        if (!expression->interval_spec ||
            !extract_interval_unit_supported(expression->interval_unit)) {
            return false;
        }
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = expression->interval_unit == MYLITE_SQL_AST_INTERVAL_UNIT_YEAR
                                     ? mylite_mysql_extract_year_display_length
                                     : mylite_mysql_temporal_part_short_display_length;
        if (expression->interval_unit == MYLITE_SQL_AST_INTERVAL_UNIT_HOUR) {
            out_descriptor->length = mylite_mysql_temporal_part_hour_display_length;
        }
        return true;
    }
    return false;
}

bool mylite_expression_descriptor_interval_unit_has_time_part(
    enum mylite_sql_ast_interval_unit unit)
{
    if (unit == MYLITE_SQL_AST_INTERVAL_UNIT_HOUR) {
        return true;
    }
    if (unit == MYLITE_SQL_AST_INTERVAL_UNIT_MINUTE) {
        return true;
    }
    return unit == MYLITE_SQL_AST_INTERVAL_UNIT_SECOND;
}

static int infer_from_unixtime_function_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *timestamp_argument = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor argument_descriptor = mylite_expression_descriptor_defaults();
    unsigned int decimals = 0U;
    int status = MYLITE_OK;

    if (!mylite_function_name_is_from_unixtime(name)) {
        return MYLITE_UNSUPPORTED;
    }
    if (mylite_sql_ast_node_child_count(arguments) == 2U) {
        *out_descriptor = from_unixtime_string_descriptor(database, value);
        return MYLITE_OK;
    }

    status = callbacks->infer_expression_descriptor(database, plan, timestamp_argument, NULL,
                                                    &argument_descriptor);
    if (status != MYLITE_OK) {
        return status;
    }
    decimals = from_unixtime_argument_decimals(timestamp_argument, &argument_descriptor);
    *out_descriptor = from_unixtime_datetime_descriptor(decimals);
    return MYLITE_OK;
}

static struct mylite_field_descriptor
from_unixtime_string_descriptor(mylite_db *database, const struct mylite_expression_value *value)
{
    uint64_t length = mylite_mysql_text_length;
    struct mylite_field_descriptor descriptor = {0};

    if (value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        length = mylite_expression_descriptor_string_length(database, value, NULL);
    }
    descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = 0U,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_charset_id(database),
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(&descriptor, true);
    return descriptor;
}

static struct mylite_field_descriptor from_unixtime_datetime_descriptor(unsigned int decimals)
{
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_DATETIME,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = decimals == 0U ? mylite_mysql_datetime_display_length
                                 : mylite_mysql_datetime_fraction_display_base + decimals,
        .decimals = decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };

    mylite_field_descriptor_set_nullable(&descriptor, true);
    return descriptor;
}

static unsigned int
from_unixtime_argument_decimals(const struct mylite_sql_ast_node *argument,
                                const struct mylite_field_descriptor *descriptor)
{
    if (from_unixtime_argument_is_approximate(argument, descriptor)) {
        return mylite_mysql_temporal_max_fsp;
    }
    if (descriptor == NULL || descriptor->type == MYLITE_FIELD_TYPE_NULL ||
        mylite_expression_descriptor_has_text_result(descriptor)) {
        return mylite_mysql_temporal_max_fsp;
    }
    if (descriptor->type == MYLITE_FIELD_TYPE_NEWDECIMAL) {
        return descriptor->decimals > mylite_mysql_temporal_max_fsp ? mylite_mysql_temporal_max_fsp
                                                                    : descriptor->decimals;
    }
    return 0U;
}

static bool from_unixtime_argument_is_approximate(const struct mylite_sql_ast_node *argument,
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
    return descriptor->type == MYLITE_FIELD_TYPE_FLOAT ||
           descriptor->type == MYLITE_FIELD_TYPE_DOUBLE;
}

static int infer_date_interval_function_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *temporal = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor temporal_descriptor = mylite_expression_descriptor_defaults();
    int status = MYLITE_OK;

    if (!mylite_function_name_is_date_interval_arithmetic(name) || !expression->interval_spec) {
        return MYLITE_UNSUPPORTED;
    }

    status = callbacks->infer_expression_descriptor(database, plan, temporal, NULL,
                                                    &temporal_descriptor);
    if (status != MYLITE_OK) {
        return status;
    }
    if (temporal_descriptor.type == MYLITE_FIELD_TYPE_DATE) {
        if (mylite_expression_descriptor_interval_unit_has_time_part(expression->interval_unit)) {
            *out_descriptor = date_interval_datetime_descriptor(0U);
        } else {
            *out_descriptor = (struct mylite_field_descriptor){
                .type = MYLITE_FIELD_TYPE_DATE,
                .flags = MYLITE_FIELD_FLAG_BINARY,
                .length = mylite_mysql_date_display_length,
                .charset_id = mylite_mysql_binary_charset_id,
                .nullable = true,
            };
            mylite_field_descriptor_set_nullable(out_descriptor, true);
        }
        return MYLITE_OK;
    }
    if (temporal_descriptor.type == MYLITE_FIELD_TYPE_DATETIME ||
        temporal_descriptor.type == MYLITE_FIELD_TYPE_TIMESTAMP) {
        *out_descriptor = date_interval_datetime_descriptor(temporal_descriptor.decimals);
        return MYLITE_OK;
    }

    *out_descriptor = date_interval_string_descriptor(database);
    return MYLITE_OK;
}

static struct mylite_field_descriptor date_interval_string_descriptor(mylite_db *database)
{
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);
    uint64_t length =
        max_bytes_per_character > UINT64_MAX / mylite_mysql_date_arithmetic_string_result_chars
            ? mylite_mysql_long_text_length
            : mylite_mysql_date_arithmetic_string_result_chars * max_bytes_per_character;
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_STRING,
        .flags = 0U,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_charset_id(database),
        .nullable = true,
    };

    mylite_field_descriptor_set_nullable(&descriptor, true);
    return descriptor;
}

static struct mylite_field_descriptor date_interval_datetime_descriptor(unsigned int decimals)
{
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_DATETIME,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = decimals == 0U ? mylite_mysql_datetime_display_length
                                 : mylite_mysql_datetime_fraction_display_base + decimals,
        .decimals = decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };

    mylite_field_descriptor_set_nullable(&descriptor, true);
    return descriptor;
}

static struct mylite_field_descriptor current_date_function_descriptor(void)
{
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_DATE,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = mylite_mysql_date_display_length,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = false,
    };

    mylite_field_descriptor_set_nullable(&descriptor, false);
    return descriptor;
}

static struct mylite_field_descriptor current_time_function_descriptor(unsigned int fsp)
{
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_TIME,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = fsp == 0U ? mylite_mysql_current_time_display_length
                            : mylite_mysql_current_time_fraction_display_base + fsp,
        .decimals = fsp,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = false,
    };

    mylite_field_descriptor_set_nullable(&descriptor, false);
    return descriptor;
}

static bool extract_interval_unit_supported(enum mylite_sql_ast_interval_unit unit)
{
    switch (unit) {
    case MYLITE_SQL_AST_INTERVAL_UNIT_YEAR:
    case MYLITE_SQL_AST_INTERVAL_UNIT_MONTH:
    case MYLITE_SQL_AST_INTERVAL_UNIT_DAY:
    case MYLITE_SQL_AST_INTERVAL_UNIT_HOUR:
    case MYLITE_SQL_AST_INTERVAL_UNIT_MINUTE:
    case MYLITE_SQL_AST_INTERVAL_UNIT_SECOND:
        return true;
    case MYLITE_SQL_AST_INTERVAL_UNIT_NONE:
    case MYLITE_SQL_AST_INTERVAL_UNIT_WEEK:
        return false;
    }
    return false;
}
