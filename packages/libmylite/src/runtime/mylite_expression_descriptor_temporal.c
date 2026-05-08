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

static int infer_str_to_date_function_descriptor(
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor
);

static struct mylite_field_descriptor str_to_date_descriptor_from_format(
    const struct mylite_sql_ast_node *format
);

static struct mylite_field_descriptor str_to_date_date_descriptor(void);

static struct mylite_field_descriptor str_to_date_time_descriptor(unsigned int decimals);

static struct mylite_field_descriptor str_to_date_datetime_descriptor(unsigned int decimals);

static void str_to_date_format_parts(
    const struct mylite_sql_ast_node *format,
    bool *out_literal,
    bool *out_has_date_part,
    bool *out_has_time_part,
    bool *out_has_fraction
);

static void str_to_date_format_token_parts(
    char token,
    bool *has_date_part,
    bool *has_time_part,
    bool *has_fraction
);

static int infer_from_unixtime_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks
);

static struct mylite_field_descriptor from_unixtime_string_descriptor(
    mylite_db *database,
    const struct mylite_expression_value *value
);

static struct mylite_field_descriptor from_unixtime_datetime_descriptor(unsigned int decimals);

static unsigned int from_unixtime_argument_decimals(
    const struct mylite_sql_ast_node *argument,
    const struct mylite_field_descriptor *descriptor
);

static bool from_unixtime_argument_is_approximate(
    const struct mylite_sql_ast_node *argument,
    const struct mylite_field_descriptor *descriptor
);

static int infer_temporal_format_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks
);

static struct mylite_field_descriptor temporal_format_string_descriptor(
    mylite_db *database,
    uint64_t character_length
);

static uint64_t temporal_format_result_character_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    bool time_format,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks
);

static uint64_t literal_temporal_format_character_length(
    const struct mylite_sql_ast_node *format,
    bool time_format
);

static uint64_t date_format_token_character_length(char token);

static uint64_t time_format_token_character_length(char token);

static uint64_t dynamic_temporal_format_character_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *format,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks
);

static int infer_date_interval_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks
);

static struct mylite_field_descriptor date_interval_string_descriptor(mylite_db *database);

static struct mylite_field_descriptor date_interval_datetime_descriptor(unsigned int decimals);

static bool extract_interval_unit_supported(enum mylite_sql_ast_interval_unit unit);

static uint64_t extract_interval_unit_display_length(enum mylite_sql_ast_interval_unit unit);

struct mylite_field_descriptor mylite_expression_descriptor_current_datetime_function(
    unsigned int fsp
) {
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
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor
) {
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
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks
) {
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
    status = infer_str_to_date_function_descriptor(expression, out_descriptor);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = infer_from_unixtime_function_descriptor(
        database,
        plan,
        expression,
        value,
        out_descriptor,
        callbacks
    );
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = infer_temporal_format_function_descriptor(
        database,
        plan,
        expression,
        out_descriptor,
        callbacks
    );
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = mylite_expression_descriptor_infer_time_function(
        database,
        plan,
        expression,
        value,
        out_descriptor,
        callbacks
    );
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = mylite_expression_descriptor_infer_sec_to_time_function(
        database,
        plan,
        expression,
        out_descriptor,
        callbacks
    );
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = mylite_expression_descriptor_infer_timediff_function(
        database,
        plan,
        expression,
        value,
        out_descriptor,
        callbacks
    );
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = mylite_expression_descriptor_infer_addsubtime_function(
        database,
        plan,
        expression,
        value,
        out_descriptor,
        callbacks
    );
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = mylite_expression_descriptor_infer_timestamp_function(
        database,
        plan,
        expression,
        value,
        out_descriptor,
        callbacks
    );
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    return infer_date_interval_function_descriptor(
        database,
        plan,
        expression,
        out_descriptor,
        callbacks
    );
}

bool mylite_expression_descriptor_infer_temporal_scalar_function(
    const struct mylite_sql_ast_node *name,
    bool arguments_nullable,
    struct mylite_field_descriptor *out_descriptor
) {
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
    if (mylite_function_name_is_last_day(name)) {
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
    if (mylite_function_name_is_week(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = mylite_mysql_temporal_part_short_display_length;
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
    if (mylite_function_name_is_dayofweek_part(name) ||
        mylite_function_name_is_quarter_part(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = mylite_mysql_temporal_part_tiny_display_length;
        return true;
    }
    if (mylite_function_name_is_dayofyear_part(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = mylite_mysql_temporal_part_dayofyear_display_length;
        return true;
    }
    if (mylite_function_name_is_hour_part(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = mylite_mysql_temporal_part_hour_display_length;
        return true;
    }
    if (mylite_function_name_is_microsecond_part(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = mylite_mysql_signed_longlong_display_length;
        return true;
    }
    if (mylite_function_name_is_time_to_sec(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = mylite_mysql_time_to_sec_function_display_length;
        return true;
    }
    return false;
}

bool mylite_expression_descriptor_infer_temporal_part_function(
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor
) {
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);

    if (mylite_function_name_is_extract(name)) {
        if (!expression->interval_spec ||
            !extract_interval_unit_supported(expression->interval_unit)) {
            return false;
        }
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = extract_interval_unit_display_length(expression->interval_unit);
        return true;
    }
    return false;
}

bool mylite_expression_descriptor_interval_unit_has_time_part(
    enum mylite_sql_ast_interval_unit unit
) {
    if (unit == MYLITE_SQL_AST_INTERVAL_UNIT_HOUR) {
        return true;
    }
    if (unit == MYLITE_SQL_AST_INTERVAL_UNIT_MINUTE) {
        return true;
    }
    if (unit == MYLITE_SQL_AST_INTERVAL_UNIT_SECOND) {
        return true;
    }
    return unit == MYLITE_SQL_AST_INTERVAL_UNIT_MICROSECOND;
}

static int infer_str_to_date_function_descriptor(
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor
) {
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *format = mylite_ast_child_at(arguments, 1U);

    if (!mylite_function_name_is_str_to_date(name)) {
        return MYLITE_UNSUPPORTED;
    }
    if (mylite_sql_ast_node_child_count(arguments) != 2U) {
        return MYLITE_UNSUPPORTED;
    }
    *out_descriptor = str_to_date_descriptor_from_format(format);
    return MYLITE_OK;
}

static struct mylite_field_descriptor str_to_date_descriptor_from_format(
    const struct mylite_sql_ast_node *format
) {
    bool literal = false;
    bool has_date_part = false;
    bool has_time_part = false;
    bool has_fraction = false;
    unsigned int decimals = 0U;

    str_to_date_format_parts(format, &literal, &has_date_part, &has_time_part, &has_fraction);
    if (!literal) {
        return str_to_date_datetime_descriptor(mylite_mysql_temporal_max_fsp);
    }

    decimals = has_fraction ? mylite_mysql_temporal_max_fsp : 0U;
    if (has_date_part && has_time_part) {
        return str_to_date_datetime_descriptor(decimals);
    }
    if (has_time_part) {
        return str_to_date_time_descriptor(decimals);
    }
    return str_to_date_date_descriptor();
}

static struct mylite_field_descriptor str_to_date_date_descriptor(void) {
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_DATE,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = mylite_mysql_date_display_length,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };

    mylite_field_descriptor_set_nullable(&descriptor, true);
    return descriptor;
}

static struct mylite_field_descriptor str_to_date_time_descriptor(unsigned int decimals) {
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

static struct mylite_field_descriptor str_to_date_datetime_descriptor(unsigned int decimals) {
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

static void str_to_date_format_parts(
    const struct mylite_sql_ast_node *format,
    bool *out_literal,
    bool *out_has_date_part,
    bool *out_has_time_part,
    bool *out_has_fraction
) {
    bool has_date_part = false;
    bool has_time_part = false;
    bool has_fraction = false;

    format = mylite_sql_ast_unwrap_parenthesized_expression(format);
    if (format == NULL || format->kind != MYLITE_SQL_AST_LITERAL ||
        (format->literal_kind != MYLITE_SQL_AST_LITERAL_STRING &&
         format->literal_kind != MYLITE_SQL_AST_LITERAL_NATIONAL_STRING)) {
        *out_literal = false;
        *out_has_date_part = false;
        *out_has_time_part = false;
        *out_has_fraction = false;
        return;
    }

    for (size_t offset = 0U; offset < format->span.length; ++offset) {
        if (format->span.text[offset] != '%' || offset + 1U >= format->span.length) {
            continue;
        }
        str_to_date_format_token_parts(
            format->span.text[++offset],
            &has_date_part,
            &has_time_part,
            &has_fraction
        );
    }
    *out_literal = true;
    *out_has_date_part = has_date_part;
    *out_has_time_part = has_time_part;
    *out_has_fraction = has_fraction;
}

static void str_to_date_format_token_parts(
    char token,
    bool *has_date_part,
    bool *has_time_part,
    bool *has_fraction
) {
    switch (token) {
    case 'Y':
    case 'y':
    case 'm':
    case 'c':
    case 'M':
    case 'b':
    case 'd':
    case 'e':
    case 'D':
    case 'j':
        *has_date_part = true;
        break;
    case 'H':
    case 'k':
    case 'h':
    case 'I':
    case 'l':
    case 'i':
    case 's':
    case 'S':
    case 'r':
    case 'T':
        *has_time_part = true;
        break;
    case 'f':
        *has_time_part = true;
        *has_fraction = true;
        break;
    default:
        break;
    }
}

static int infer_from_unixtime_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks
) {
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

    status = callbacks->infer_expression_descriptor(
        database,
        plan,
        timestamp_argument,
        NULL,
        &argument_descriptor
    );
    if (status != MYLITE_OK) {
        return status;
    }
    decimals = from_unixtime_argument_decimals(timestamp_argument, &argument_descriptor);
    *out_descriptor = from_unixtime_datetime_descriptor(decimals);
    return MYLITE_OK;
}

static struct mylite_field_descriptor from_unixtime_string_descriptor(
    mylite_db *database,
    const struct mylite_expression_value *value
) {
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

static struct mylite_field_descriptor from_unixtime_datetime_descriptor(unsigned int decimals) {
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

static unsigned int from_unixtime_argument_decimals(
    const struct mylite_sql_ast_node *argument,
    const struct mylite_field_descriptor *descriptor
) {
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

static bool from_unixtime_argument_is_approximate(
    const struct mylite_sql_ast_node *argument,
    const struct mylite_field_descriptor *descriptor
) {
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

static int infer_temporal_format_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    bool time_format = false;
    uint64_t character_length = 0U;

    if (name != NULL && mylite_span_equal_ci(name->span, "TIME_FORMAT")) {
        time_format = true;
    } else if (name == NULL || !mylite_span_equal_ci(name->span, "DATE_FORMAT")) {
        return MYLITE_UNSUPPORTED;
    }

    character_length =
        temporal_format_result_character_length(database, plan, expression, time_format, callbacks);
    *out_descriptor = temporal_format_string_descriptor(database, character_length);
    return MYLITE_OK;
}

static struct mylite_field_descriptor temporal_format_string_descriptor(
    mylite_db *database,
    uint64_t character_length
) {
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);
    uint64_t length =
        max_bytes_per_character != 0U && character_length > UINT64_MAX / max_bytes_per_character
            ? mylite_mysql_long_text_length
            : character_length * max_bytes_per_character;
    struct mylite_field_descriptor descriptor = {
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

static uint64_t temporal_format_result_character_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    bool time_format,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *format = mylite_ast_child_at(arguments, 1U);
    uint64_t length = literal_temporal_format_character_length(format, time_format);

    if (length != 0U) {
        return length;
    }
    return dynamic_temporal_format_character_length(database, plan, format, callbacks);
}

static uint64_t literal_temporal_format_character_length(
    const struct mylite_sql_ast_node *format,
    bool time_format
) {
    uint64_t length = 0U;
    bool quoted = false;

    format = mylite_sql_ast_unwrap_parenthesized_expression(format);
    if (format == NULL || format->kind != MYLITE_SQL_AST_LITERAL ||
        (format->literal_kind != MYLITE_SQL_AST_LITERAL_STRING &&
         format->literal_kind != MYLITE_SQL_AST_LITERAL_NATIONAL_STRING)) {
        return 0U;
    }
    quoted = format->span.length >= 2U && format->span.text[0] == '\'' &&
             format->span.text[format->span.length - 1U] == '\'';

    for (size_t offset = 0U; offset < format->span.length; ++offset) {
        uint64_t token_length = 1U;

        if (format->span.text[offset] == '%' && offset + 1U < format->span.length) {
            char token = format->span.text[++offset];

            token_length = time_format ? time_format_token_character_length(token)
                                       : date_format_token_character_length(token);
        }
        if (length > UINT64_MAX - token_length) {
            return mylite_mysql_long_text_length;
        }
        length += token_length;
    }
    return quoted && length >= 2U ? length - 2U : length;
}

static uint64_t date_format_token_character_length(char token) {
    switch (token) {
    case 'M':
    case 'W':
    case 'r':
        return 9U;
    case 'T':
        return 8U;
    case 'f':
        return 6U;
    case 'D':
    case 'X':
    case 'x':
    case 'Y':
        return 4U;
    case 'a':
    case 'b':
    case 'j':
        return 3U;
    case 'c':
    case 'd':
    case 'e':
    case 'H':
    case 'h':
    case 'I':
    case 'i':
    case 'k':
    case 'l':
    case 'm':
    case 'p':
    case 'S':
    case 's':
    case 'U':
    case 'u':
    case 'V':
    case 'v':
    case 'y':
        return 2U;
    default:
        return 1U;
    }
}

static uint64_t time_format_token_character_length(char token) {
    switch (token) {
    case 'r':
        return 13U;
    case 'T':
        return 10U;
    case 'H':
    case 'k':
        return 7U;
    case 'f':
        return 6U;
    case 'Y':
        return 4U;
    case 'h':
    case 'I':
    case 'i':
    case 'm':
    case 'p':
    case 'S':
    case 's':
    case 'y':
        return 2U;
    default:
        return 1U;
    }
}

static uint64_t dynamic_temporal_format_character_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *format,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks
) {
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);

    if (callbacks->infer_expression_descriptor(database, plan, format, NULL, &descriptor) !=
        MYLITE_OK) {
        return mylite_mysql_text_length;
    }
    if (descriptor.length > UINT64_MAX / 10U) {
        return mylite_mysql_long_text_length;
    }
    if (max_bytes_per_character > 1U) {
        return (descriptor.length / max_bytes_per_character) * 10U;
    }
    return descriptor.length * 10U;
}

static int infer_date_interval_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_temporal_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *temporal = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor temporal_descriptor = mylite_expression_descriptor_defaults();
    int status = MYLITE_OK;

    if (!mylite_function_name_is_date_interval_arithmetic(name) || !expression->interval_spec) {
        return MYLITE_UNSUPPORTED;
    }

    status =
        callbacks
            ->infer_expression_descriptor(database, plan, temporal, NULL, &temporal_descriptor);
    if (status != MYLITE_OK) {
        return status;
    }
    if (temporal_descriptor.type == MYLITE_FIELD_TYPE_DATE) {
        if (mylite_expression_descriptor_interval_unit_has_time_part(expression->interval_unit)) {
            unsigned int decimals =
                expression->interval_unit == MYLITE_SQL_AST_INTERVAL_UNIT_MICROSECOND
                    ? mylite_mysql_temporal_max_fsp
                    : 0U;

            *out_descriptor = date_interval_datetime_descriptor(decimals);
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
        unsigned int decimals =
            expression->interval_unit == MYLITE_SQL_AST_INTERVAL_UNIT_MICROSECOND
                ? mylite_mysql_temporal_max_fsp
                : temporal_descriptor.decimals;

        *out_descriptor = date_interval_datetime_descriptor(decimals);
        return MYLITE_OK;
    }

    *out_descriptor = date_interval_string_descriptor(database);
    return MYLITE_OK;
}

static struct mylite_field_descriptor date_interval_string_descriptor(mylite_db *database) {
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

static struct mylite_field_descriptor date_interval_datetime_descriptor(unsigned int decimals) {
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

static struct mylite_field_descriptor current_date_function_descriptor(void) {
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

static struct mylite_field_descriptor current_time_function_descriptor(unsigned int fsp) {
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

static bool extract_interval_unit_supported(enum mylite_sql_ast_interval_unit unit) {
    switch (unit) {
    case MYLITE_SQL_AST_INTERVAL_UNIT_YEAR:
    case MYLITE_SQL_AST_INTERVAL_UNIT_MONTH:
    case MYLITE_SQL_AST_INTERVAL_UNIT_DAY:
    case MYLITE_SQL_AST_INTERVAL_UNIT_QUARTER:
    case MYLITE_SQL_AST_INTERVAL_UNIT_HOUR:
    case MYLITE_SQL_AST_INTERVAL_UNIT_MINUTE:
    case MYLITE_SQL_AST_INTERVAL_UNIT_SECOND:
    case MYLITE_SQL_AST_INTERVAL_UNIT_MICROSECOND:
        return true;
    case MYLITE_SQL_AST_INTERVAL_UNIT_NONE:
    case MYLITE_SQL_AST_INTERVAL_UNIT_WEEK:
        return false;
    }
    return false;
}

static uint64_t extract_interval_unit_display_length(enum mylite_sql_ast_interval_unit unit) {
    switch (unit) {
    case MYLITE_SQL_AST_INTERVAL_UNIT_YEAR:
        return mylite_mysql_extract_year_display_length;
    case MYLITE_SQL_AST_INTERVAL_UNIT_QUARTER:
        return mylite_mysql_temporal_part_tiny_display_length;
    case MYLITE_SQL_AST_INTERVAL_UNIT_HOUR:
        return mylite_mysql_temporal_part_hour_display_length;
    case MYLITE_SQL_AST_INTERVAL_UNIT_MICROSECOND:
        return mylite_mysql_extract_microsecond_display_length;
    case MYLITE_SQL_AST_INTERVAL_UNIT_MONTH:
    case MYLITE_SQL_AST_INTERVAL_UNIT_DAY:
    case MYLITE_SQL_AST_INTERVAL_UNIT_MINUTE:
    case MYLITE_SQL_AST_INTERVAL_UNIT_SECOND:
    case MYLITE_SQL_AST_INTERVAL_UNIT_NONE:
    case MYLITE_SQL_AST_INTERVAL_UNIT_WEEK:
        return mylite_mysql_temporal_part_short_display_length;
    }
    return mylite_mysql_temporal_part_short_display_length;
}
