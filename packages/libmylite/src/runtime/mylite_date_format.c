#include "mylite_date_format.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_warning_incorrect_datetime_value = 1292,
    date_year_minimum = 1000,
    date_year_maximum = 9999,
    date_months_per_year = 12,
    date_months_after_february = 9,
    date_first_month = 1,
    date_first_day = 1,
    date_leap_day = 29,
    date_february = 2,
    date_text_length = 10,
    datetime_text_length = 19,
    date_year_month_separator_index = 4,
    date_month_day_separator_index = 7,
    date_month_text_offset = 5,
    date_day_text_offset = 8,
    datetime_date_time_separator_index = 10,
    datetime_hour_minute_separator_index = 13,
    datetime_minute_second_separator_index = 16,
    datetime_hour_text_offset = 11,
    datetime_minute_text_offset = 14,
    datetime_second_text_offset = 17,
    date_add_march_year_shift_month = 2,
    date_add_month_scale = 153,
    date_add_month_bias = 2,
    date_add_month_divisor = 5,
    date_add_days_per_non_leap_year = 365,
    date_add_days_per_era = 146097,
    date_add_unix_epoch_day_offset = 719468,
    date_weekday_unix_epoch_thursday = 4,
    date_two_digit_modulus = 100,
    date_format_int_text_capacity = 16,
    date_format_initial_capacity = 64,
    date_digit_radix = 10,
    date_hours_per_half_day = 12,
    time_hour_max = 23,
    time_minute_second_max = 59,
    date_leap_year_quadrennial_cycle = 4,
    date_gregorian_century = 100,
    date_gregorian_era = 400,
    date_gregorian_era_adjustment = 399,
    days_per_week = 7,
    ordinal_teen_suffix_minimum = 11,
    ordinal_teen_suffix_maximum = 13,
};

struct date_format_parts {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
};

struct date_format_buffer {
    char *data;
    size_t length;
    size_t capacity;
};

struct date_format_time {
    int hour;
    int minute;
    int second;
};

struct date_format_input {
    const char *value;
    size_t value_length;
    enum mylite_date_format_input_kind input_kind;
};

static void date_format_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static int date_format_sqlite_input_kind(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_date_format_input_kind *out_kind
);
static int date_format_sqlite_result(
    sqlite3_context *context,
    const char *value,
    size_t value_length,
    enum mylite_date_format_input_kind input_kind,
    const char *format,
    size_t format_length
);

static int format_date_value(
    struct mylite_db *database,
    const struct date_format_parts *parts,
    const char *format,
    size_t format_length,
    char **out_text
);
static int append_date_format_token(
    struct mylite_db *database,
    struct date_format_buffer *buffer,
    const struct date_format_parts *parts,
    char token
);
static int append_two_digit(struct date_format_buffer *buffer, int value);
static int append_three_digit(struct date_format_buffer *buffer, int value);
static int append_unpadded_int(struct date_format_buffer *buffer, int value);
static int append_time_24(struct date_format_buffer *buffer, const struct date_format_time *time);
static int append_time_12(struct date_format_buffer *buffer, const struct date_format_time *time);
static int append_ordinal_day(struct date_format_buffer *buffer, int day);
static int append_date_format_text(struct date_format_buffer *buffer, const char *text);
static int append_date_format_byte(struct date_format_buffer *buffer, char value);
static int reserve_date_format_buffer(struct date_format_buffer *buffer, size_t extra_length);
static void date_format_buffer_deinit(struct date_format_buffer *buffer);

static bool parse_date_format_input(
    const struct date_format_input *input,
    struct date_format_parts *out_parts
);
static bool parse_date_text(
    const char *value,
    size_t value_length,
    struct date_format_parts *out_parts
);
static bool parse_datetime_text(
    const char *value,
    size_t value_length,
    struct date_format_parts *out_parts
);
static bool parse_two_digits(const char *text, int *out_value);
static bool parse_four_digits(const char *text, int *out_value);
static bool date_parts_are_valid(const struct date_format_parts *parts);
static bool date_time_parts_are_valid(const struct date_format_parts *parts);
static bool is_leap_year(int year);
static int days_in_month(int year, int month);
static int day_of_year(const struct date_format_parts *parts);
static int weekday_sunday_zero(const struct date_format_parts *parts);
static int64_t days_from_civil(const struct date_format_parts *parts);
static int date_format_hour_12(int hour);
static const char *month_name(int month);
static const char *month_abbreviation(int month);
static const char *weekday_name(int weekday);
static const char *weekday_abbreviation(int weekday);
static const char *ordinal_suffix(int day);
static bool format_token_is_deferred_week_token(char token);
static void set_date_format_unsupported_error(struct mylite_db *database, const char *message);
static int append_incorrect_datetime_warning(
    struct mylite_db *database,
    const char *value,
    size_t value_length
);

const char *mylite_date_format_input_kind_name(enum mylite_date_format_input_kind kind) {
    switch (kind) {
    case MYLITE_DATE_FORMAT_INPUT_STRING:
        return "string";
    case MYLITE_DATE_FORMAT_INPUT_DATE:
        return "date";
    case MYLITE_DATE_FORMAT_INPUT_DATETIME:
        return "datetime";
    case MYLITE_DATE_FORMAT_INPUT_TIMESTAMP:
        return "timestamp";
    }
    return NULL;
}

bool mylite_date_format_input_kind_from_name(
    const char *name,
    size_t name_length,
    enum mylite_date_format_input_kind *out_kind
) {
    static const struct {
        const char *name;
        enum mylite_date_format_input_kind kind;
    } names[] = {
        {"string", MYLITE_DATE_FORMAT_INPUT_STRING},
        {"date", MYLITE_DATE_FORMAT_INPUT_DATE},
        {"datetime", MYLITE_DATE_FORMAT_INPUT_DATETIME},
        {"timestamp", MYLITE_DATE_FORMAT_INPUT_TIMESTAMP},
    };

    if (name == NULL || out_kind == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        size_t current_length = strlen(names[index].name);

        if (name_length == current_length && memcmp(name, names[index].name, name_length) == 0) {
            *out_kind = names[index].kind;
            return true;
        }
    }
    return false;
}

int mylite_date_format_validate_format(
    struct mylite_db *database,
    const char *format,
    size_t format_length
) {
    if (format == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t index = 0U; index < format_length; ++index) {
        if (format[index] != '%') {
            continue;
        }
        if (index + 1U >= format_length) {
            continue;
        }
        ++index;
        if (format_token_is_deferred_week_token(format[index])) {
            set_date_format_unsupported_error(
                database,
                "DATE_FORMAT() does not yet support week-based format specifiers"
            );
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

int mylite_date_format_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    enum mylite_date_format_input_kind input_kind,
    const char *format,
    size_t format_length,
    char **out_text,
    bool *out_is_null
) {
    const struct date_format_input input = {
        .value = value,
        .value_length = value_length,
        .input_kind = input_kind,
    };
    struct date_format_parts parts = {0};
    int rc = MYLITE_OK;

    if (out_text == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_is_null = false;
    if (value == NULL || format == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    rc = mylite_date_format_validate_format(database, format, format_length);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!parse_date_format_input(&input, &parts)) {
        rc = append_incorrect_datetime_warning(database, value, value_length);
        if (rc == MYLITE_OK) {
            *out_is_null = true;
        }
        return rc;
    }

    return format_date_value(database, &parts, format, format_length, out_text);
}

int mylite_sqlite_register_date_format_function(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_date_format",
            .argument_count = 3,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = date_format_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
    };

    return mylite_sqlite_register_functions(
        sqlite,
        registrations,
        sizeof(registrations) / sizeof(registrations[0])
    );
}

static void date_format_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    enum mylite_date_format_input_kind input_kind = MYLITE_DATE_FORMAT_INPUT_STRING;
    const unsigned char *value = NULL;
    const unsigned char *format = NULL;
    int value_length = 0;
    int format_length = 0;

    if (context == NULL || argc != 3 || argv == NULL || argv[0] == NULL || argv[1] == NULL ||
        argv[2] == NULL) {
        sqlite3_result_error(context, "invalid MyLite DATE_FORMAT callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL ||
        sqlite3_value_type(argv[2]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    if (date_format_sqlite_input_kind(context, argv[2], &input_kind) != MYLITE_OK) {
        return;
    }

    value = sqlite3_value_text(argv[0]);
    format = sqlite3_value_text(argv[1]);
    value_length = sqlite3_value_bytes(argv[0]);
    format_length = sqlite3_value_bytes(argv[1]);
    if (value == NULL || format == NULL || value_length < 0 || format_length < 0) {
        sqlite3_result_error_nomem(context);
        return;
    }

    if (date_format_sqlite_result(
            context,
            (const char *)value,
            (size_t)value_length,
            input_kind,
            (const char *)format,
            (size_t)format_length
        ) != MYLITE_OK) {
        return;
    }
}

static int date_format_sqlite_input_kind(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_date_format_input_kind *out_kind
) {
    const unsigned char *kind_text = sqlite3_value_text(value);
    int kind_length = sqlite3_value_bytes(value);

    if (kind_text == NULL || kind_length < 0 || out_kind == NULL) {
        sqlite3_result_error_nomem(context);
        return MYLITE_NOMEM;
    }
    if (!mylite_date_format_input_kind_from_name(
            (const char *)kind_text,
            (size_t)kind_length,
            out_kind
        )) {
        sqlite3_result_error(context, "invalid MyLite DATE_FORMAT input kind", -1);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int date_format_sqlite_result(
    sqlite3_context *context,
    const char *value,
    size_t value_length,
    enum mylite_date_format_input_kind input_kind,
    const char *format,
    size_t format_length
) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    char *result = NULL;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite DATE_FORMAT owner", -1);
        return MYLITE_ERROR;
    }

    rc = mylite_date_format_value(
        database,
        value,
        value_length,
        input_kind,
        format,
        format_length,
        &result,
        &is_null
    );
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
        } else {
            sqlite3_result_error(context, "MyLite DATE_FORMAT failed", -1);
        }
        free(result);
        return rc;
    }
    if (is_null) {
        sqlite3_result_null(context);
    } else {
        sqlite3_result_text(context, result, -1, SQLITE_TRANSIENT);
    }
    free(result);
    return MYLITE_OK;
}

static int format_date_value(
    struct mylite_db *database,
    const struct date_format_parts *parts,
    const char *format,
    size_t format_length,
    char **out_text
) {
    struct date_format_buffer buffer = {0};
    int rc = MYLITE_OK;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    for (size_t index = 0U; rc == MYLITE_OK && index < format_length; ++index) {
        if (format[index] != '%') {
            rc = append_date_format_byte(&buffer, format[index]);
            continue;
        }
        if (index + 1U >= format_length) {
            rc = append_date_format_byte(&buffer, '%');
            continue;
        }
        ++index;
        rc = append_date_format_token(database, &buffer, parts, format[index]);
    }
    if (rc == MYLITE_OK) {
        rc = append_date_format_byte(&buffer, '\0');
    }
    if (rc == MYLITE_OK) {
        *out_text = buffer.data;
        buffer = (struct date_format_buffer){0};
    }
    date_format_buffer_deinit(&buffer);
    return rc;
}

static int append_date_format_token(
    struct mylite_db *database,
    struct date_format_buffer *buffer,
    const struct date_format_parts *parts,
    char token
) {
    const struct date_format_time time = {
        .hour = parts->hour,
        .minute = parts->minute,
        .second = parts->second,
    };
    int hour12 = date_format_hour_12(parts->hour);

    switch (token) {
    case 'Y':
        return append_unpadded_int(buffer, parts->year);
    case 'y':
        return append_two_digit(buffer, parts->year % date_two_digit_modulus);
    case 'm':
        return append_two_digit(buffer, parts->month);
    case 'c':
        return append_unpadded_int(buffer, parts->month);
    case 'd':
        return append_two_digit(buffer, parts->day);
    case 'e':
        return append_unpadded_int(buffer, parts->day);
    case 'H':
        return append_two_digit(buffer, parts->hour);
    case 'k':
        return append_unpadded_int(buffer, parts->hour);
    case 'h':
    case 'I':
        return append_two_digit(buffer, hour12);
    case 'l':
        return append_unpadded_int(buffer, hour12);
    case 'i':
        return append_two_digit(buffer, parts->minute);
    case 'S':
    case 's':
        return append_two_digit(buffer, parts->second);
    case 'T':
        return append_time_24(buffer, &time);
    case 'r':
        return append_time_12(buffer, &time);
    case 'p':
        return append_date_format_text(buffer, parts->hour < date_hours_per_half_day ? "AM" : "PM");
    case 'f':
        return append_date_format_text(buffer, "000000");
    case 'a':
        return append_date_format_text(buffer, weekday_abbreviation(weekday_sunday_zero(parts)));
    case 'W':
        return append_date_format_text(buffer, weekday_name(weekday_sunday_zero(parts)));
    case 'b':
        return append_date_format_text(buffer, month_abbreviation(parts->month));
    case 'M':
        return append_date_format_text(buffer, month_name(parts->month));
    case 'D':
        return append_ordinal_day(buffer, parts->day);
    case 'j':
        return append_three_digit(buffer, day_of_year(parts));
    case 'w':
        return append_unpadded_int(buffer, weekday_sunday_zero(parts));
    case '%':
        return append_date_format_byte(buffer, '%');
    default:
        if (format_token_is_deferred_week_token(token)) {
            set_date_format_unsupported_error(
                database,
                "DATE_FORMAT() does not yet support week-based format specifiers"
            );
            return MYLITE_ERROR;
        }
        return append_date_format_byte(buffer, token);
    }
}

static int append_two_digit(struct date_format_buffer *buffer, int value) {
    char text[3];
    int written = snprintf(text, sizeof(text), "%02d", value);

    if (written < 0 || (size_t)written >= sizeof(text)) {
        return MYLITE_NOMEM;
    }
    return append_date_format_text(buffer, text);
}

static int append_three_digit(struct date_format_buffer *buffer, int value) {
    char text[4];
    int written = snprintf(text, sizeof(text), "%03d", value);

    if (written < 0 || (size_t)written >= sizeof(text)) {
        return MYLITE_NOMEM;
    }
    return append_date_format_text(buffer, text);
}

static int append_unpadded_int(struct date_format_buffer *buffer, int value) {
    char text[date_format_int_text_capacity];
    int written = snprintf(text, sizeof(text), "%d", value);

    if (written < 0 || (size_t)written >= sizeof(text)) {
        return MYLITE_NOMEM;
    }
    return append_date_format_text(buffer, text);
}

static int append_time_24(struct date_format_buffer *buffer, const struct date_format_time *time) {
    int rc = MYLITE_OK;

    if (time == NULL) {
        return MYLITE_MISUSE;
    }
    rc = append_two_digit(buffer, time->hour);

    if (rc == MYLITE_OK) {
        rc = append_date_format_byte(buffer, ':');
    }
    if (rc == MYLITE_OK) {
        rc = append_two_digit(buffer, time->minute);
    }
    if (rc == MYLITE_OK) {
        rc = append_date_format_byte(buffer, ':');
    }
    if (rc == MYLITE_OK) {
        rc = append_two_digit(buffer, time->second);
    }
    return rc;
}

static int append_time_12(struct date_format_buffer *buffer, const struct date_format_time *time) {
    int rc = MYLITE_OK;

    if (time == NULL) {
        return MYLITE_MISUSE;
    }
    rc = append_two_digit(buffer, date_format_hour_12(time->hour));

    if (rc == MYLITE_OK) {
        rc = append_date_format_byte(buffer, ':');
    }
    if (rc == MYLITE_OK) {
        rc = append_two_digit(buffer, time->minute);
    }
    if (rc == MYLITE_OK) {
        rc = append_date_format_byte(buffer, ':');
    }
    if (rc == MYLITE_OK) {
        rc = append_two_digit(buffer, time->second);
    }
    if (rc == MYLITE_OK) {
        rc = append_date_format_text(buffer, time->hour < date_hours_per_half_day ? " AM" : " PM");
    }
    return rc;
}

static int append_ordinal_day(struct date_format_buffer *buffer, int day) {
    int rc = append_unpadded_int(buffer, day);

    if (rc == MYLITE_OK) {
        rc = append_date_format_text(buffer, ordinal_suffix(day));
    }
    return rc;
}

static int append_date_format_text(struct date_format_buffer *buffer, const char *text) {
    size_t length = strlen(text);
    int rc = reserve_date_format_buffer(buffer, length);

    if (rc != MYLITE_OK) {
        return rc;
    }
    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    return MYLITE_OK;
}

static int append_date_format_byte(struct date_format_buffer *buffer, char value) {
    int rc = reserve_date_format_buffer(buffer, 1U);

    if (rc != MYLITE_OK) {
        return rc;
    }
    buffer->data[buffer->length] = value;
    ++buffer->length;
    return MYLITE_OK;
}

static int reserve_date_format_buffer(struct date_format_buffer *buffer, size_t extra_length) {
    size_t required = 0U;
    size_t capacity = 0U;
    char *data = NULL;

    if (buffer == NULL || extra_length > SIZE_MAX - buffer->length) {
        return MYLITE_NOMEM;
    }
    required = buffer->length + extra_length;
    if (required <= buffer->capacity) {
        return MYLITE_OK;
    }
    capacity = buffer->capacity == 0U ? (size_t)date_format_initial_capacity : buffer->capacity;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2U) {
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    data = (char *)realloc(buffer->data, capacity);
    if (data == NULL) {
        return MYLITE_NOMEM;
    }
    buffer->data = data;
    buffer->capacity = capacity;
    return MYLITE_OK;
}

static void date_format_buffer_deinit(struct date_format_buffer *buffer) {
    if (buffer == NULL) {
        return;
    }
    free(buffer->data);
    *buffer = (struct date_format_buffer){0};
}

static bool parse_date_format_input(
    const struct date_format_input *input,
    struct date_format_parts *out_parts
) {
    if (input == NULL) {
        return false;
    }

    switch (input->input_kind) {
    case MYLITE_DATE_FORMAT_INPUT_STRING:
        return (parse_date_text(input->value, input->value_length, out_parts) ||
                parse_datetime_text(input->value, input->value_length, out_parts)) != 0;
    case MYLITE_DATE_FORMAT_INPUT_DATE:
        return parse_date_text(input->value, input->value_length, out_parts);
    case MYLITE_DATE_FORMAT_INPUT_DATETIME:
    case MYLITE_DATE_FORMAT_INPUT_TIMESTAMP:
        return parse_datetime_text(input->value, input->value_length, out_parts);
    }
    return false;
}

static bool parse_date_text(
    const char *value,
    size_t value_length,
    struct date_format_parts *out_parts
) {
    struct date_format_parts parts = {0};

    if (value == NULL || out_parts == NULL || value_length != date_text_length ||
        value[date_year_month_separator_index] != '-' ||
        value[date_month_day_separator_index] != '-') {
        return false;
    }
    if (!parse_four_digits(value, &parts.year) ||
        !parse_two_digits(value + date_month_text_offset, &parts.month) ||
        !parse_two_digits(value + date_day_text_offset, &parts.day)) {
        return false;
    }
    if (!date_parts_are_valid(&parts)) {
        return false;
    }
    *out_parts = parts;
    return true;
}

static bool parse_datetime_text(
    const char *value,
    size_t value_length,
    struct date_format_parts *out_parts
) {
    struct date_format_parts parts = {0};

    if (value == NULL || out_parts == NULL || value_length != datetime_text_length ||
        value[datetime_date_time_separator_index] != ' ' ||
        value[datetime_hour_minute_separator_index] != ':' ||
        value[datetime_minute_second_separator_index] != ':') {
        return false;
    }
    if (!parse_date_text(value, date_text_length, &parts) ||
        !parse_two_digits(value + datetime_hour_text_offset, &parts.hour) ||
        !parse_two_digits(value + datetime_minute_text_offset, &parts.minute) ||
        !parse_two_digits(value + datetime_second_text_offset, &parts.second)) {
        return false;
    }
    if (!date_time_parts_are_valid(&parts)) {
        return false;
    }
    *out_parts = parts;
    return true;
}

static bool parse_two_digits(const char *text, int *out_value) {
    if (text == NULL || out_value == NULL || !isdigit((unsigned char)text[0]) ||
        !isdigit((unsigned char)text[1])) {
        return false;
    }
    *out_value = ((int)(text[0] - '0') * date_digit_radix) + (int)(text[1] - '0');
    return true;
}

static bool parse_four_digits(const char *text, int *out_value) {
    int value = 0;

    if (text == NULL || out_value == NULL) {
        return false;
    }
    for (size_t index = 0U; index < 4U; ++index) {
        if (!isdigit((unsigned char)text[index])) {
            return false;
        }
        value = (value * date_digit_radix) + (int)(text[index] - '0');
    }
    *out_value = value;
    return true;
}

static bool date_parts_are_valid(const struct date_format_parts *parts) {
    if (parts == NULL || parts->year < date_year_minimum || parts->year > date_year_maximum ||
        parts->month < date_first_month || parts->month > date_months_per_year ||
        parts->day < date_first_day || parts->day > days_in_month(parts->year, parts->month)) {
        return false;
    }
    return true;
}

static bool date_time_parts_are_valid(const struct date_format_parts *parts) {
    if (!date_parts_are_valid(parts)) {
        return false;
    }
    return (parts->hour >= 0 && parts->hour <= time_hour_max && parts->minute >= 0 &&
            parts->minute <= time_minute_second_max && parts->second >= 0 &&
            parts->second <= time_minute_second_max) != 0;
}

static bool is_leap_year(int year) {
    return ((year % date_leap_year_quadrennial_cycle == 0 && year % date_gregorian_century != 0) ||
            year % date_gregorian_era == 0) != 0;
}

static int days_in_month(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == date_february && is_leap_year(year)) {
        return date_leap_day;
    }
    if (month < date_first_month || month > date_months_per_year) {
        return 0;
    }
    return days[month - 1];
}

static int day_of_year(const struct date_format_parts *parts) {
    int day = parts->day;

    for (int month = 1; month < parts->month; ++month) {
        day += days_in_month(parts->year, month);
    }
    return day;
}

static int weekday_sunday_zero(const struct date_format_parts *parts) {
    int64_t days = days_from_civil(parts);
    int weekday = (int)((days + date_weekday_unix_epoch_thursday) % days_per_week);

    if (weekday < 0) {
        weekday += days_per_week;
    }
    return weekday;
}

static int64_t days_from_civil(const struct date_format_parts *parts) {
    unsigned int month = (unsigned int)parts->month;
    unsigned int day = (unsigned int)parts->day;
    int adjusted_year =
        parts->year - (month <= (unsigned int)date_add_march_year_shift_month ? 1 : 0);
    int era = adjusted_year >= 0
                  ? adjusted_year / date_gregorian_era
                  : (adjusted_year - date_gregorian_era_adjustment) / date_gregorian_era;
    unsigned int year_of_era = (unsigned int)(adjusted_year - (era * date_gregorian_era));
    unsigned int month_prime = month > (unsigned int)date_add_march_year_shift_month
                                   ? month - ((unsigned int)date_add_march_year_shift_month + 1U)
                                   : month + (unsigned int)date_months_after_february;
    unsigned int day_of_year_value =
        (((date_add_month_scale * month_prime) + date_add_month_bias) / date_add_month_divisor) +
        day - 1U;
    unsigned int day_of_era = (year_of_era * date_add_days_per_non_leap_year) +
                              (year_of_era / (unsigned int)date_leap_year_quadrennial_cycle) -
                              (year_of_era / (unsigned int)date_gregorian_century) +
                              day_of_year_value;

    return ((int64_t)era * date_add_days_per_era) + (int64_t)day_of_era -
           date_add_unix_epoch_day_offset;
}

static int date_format_hour_12(int hour) {
    int value = hour % date_hours_per_half_day;

    return value == 0 ? date_hours_per_half_day : value;
}

static const char *month_name(int month) {
    static const char *const names[] = {
        "January",
        "February",
        "March",
        "April",
        "May",
        "June",
        "July",
        "August",
        "September",
        "October",
        "November",
        "December",
    };

    return names[month - 1];
}

static const char *month_abbreviation(int month) {
    static const char *const names[] = {
        "Jan",
        "Feb",
        "Mar",
        "Apr",
        "May",
        "Jun",
        "Jul",
        "Aug",
        "Sep",
        "Oct",
        "Nov",
        "Dec",
    };

    return names[month - 1];
}

static const char *weekday_name(int weekday) {
    static const char *const names[] = {
        "Sunday",
        "Monday",
        "Tuesday",
        "Wednesday",
        "Thursday",
        "Friday",
        "Saturday",
    };

    return names[weekday];
}

static const char *weekday_abbreviation(int weekday) {
    static const char *const names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    return names[weekday];
}

static const char *ordinal_suffix(int day) {
    int last_two_digits = day % date_two_digit_modulus;
    int last_digit = day % date_digit_radix;

    if (last_two_digits >= ordinal_teen_suffix_minimum &&
        last_two_digits <= ordinal_teen_suffix_maximum) {
        return "th";
    }
    if (last_digit == 1) {
        return "st";
    }
    if (last_digit == 2) {
        return "nd";
    }
    if (last_digit == 3) {
        return "rd";
    }
    return "th";
}

static bool format_token_is_deferred_week_token(char token) {
    return (token == 'U' || token == 'u' || token == 'V' || token == 'v' || token == 'X' ||
            token == 'x') != 0;
}

static void set_date_format_unsupported_error(struct mylite_db *database, const char *message) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_parse,
        "42000",
        message
    );
}

static int append_incorrect_datetime_warning(
    struct mylite_db *database,
    const char *value,
    size_t value_length
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect datetime value: '%.*s'",
        value_length > 200U ? 200 : (int)value_length,
        value == NULL ? "" : value
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_incorrect_datetime_value,
        "22007",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_NOMEM,
            "HY001",
            "out of memory"
        );
    }
    return rc;
}
