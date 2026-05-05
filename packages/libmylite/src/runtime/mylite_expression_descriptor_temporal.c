#include "mylite_expression_descriptor_temporal.h"

#include "mylite_expression_descriptor.h"
#include "mylite_function_names.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"
#include "mylite_temporal_functions.h"

static struct mylite_field_descriptor current_date_function_descriptor(void);
static struct mylite_field_descriptor current_time_function_descriptor(unsigned int fsp);
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
