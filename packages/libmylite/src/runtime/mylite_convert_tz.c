#include "mylite_convert_tz.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    mysql_error_internal = 1105,
    mysql_warning_incorrect_datetime_value = 1292,
    convert_tz_datetime_length = 19,
    convert_tz_text_capacity = convert_tz_datetime_length + 1,
    convert_tz_offset_length = 6,
    convert_tz_datetime_year_start = 0,
    convert_tz_datetime_first_date_separator = 4,
    convert_tz_datetime_month_start = 5,
    convert_tz_datetime_second_date_separator = 7,
    convert_tz_datetime_day_start = 8,
    convert_tz_datetime_date_time_separator = 10,
    convert_tz_datetime_hour_start = 11,
    convert_tz_datetime_first_time_separator = 13,
    convert_tz_datetime_minute_start = 14,
    convert_tz_datetime_second_time_separator = 16,
    convert_tz_datetime_second_start = 17,
    convert_tz_offset_sign_index = 0,
    convert_tz_offset_hour_start = 1,
    convert_tz_offset_separator_index = 3,
    convert_tz_offset_minute_start = 4,
    convert_tz_max_positive_offset_minutes = 14 * 60,
    convert_tz_max_negative_offset_minutes = (13 * 60) + 59,
    convert_tz_decimal_base = 10,
    convert_tz_seconds_per_minute = 60,
    convert_tz_minutes_per_hour = 60,
    convert_tz_hours_per_day = 24,
    convert_tz_seconds_per_hour = 60 * 60,
    convert_tz_seconds_per_day = 24 * 60 * 60,
    convert_tz_date_year_min = 1,
    convert_tz_date_year_max = 9999,
    convert_tz_date_month_min = 1,
    convert_tz_date_month_max = 12,
    convert_tz_date_day_min = 1,
    convert_tz_time_min = 0,
    convert_tz_hour_max = 23,
    convert_tz_minute_second_max = 59,
    convert_tz_february = 2,
    convert_tz_february_days_leap = 29,
    convert_tz_leap_year_cycle_short = 4,
    convert_tz_leap_year_cycle_century = 100,
    convert_tz_leap_year_cycle_full = 400,
    convert_tz_printable_ascii_min = 0x20U,
    convert_tz_printable_ascii_max = 0x7eU,
    convert_tz_warning_input_capacity = 96,
    civil_march_year_start_month = 3,
    civil_march_year_end_adjustment = 9,
    civil_march_year_month_split = 10,
    civil_era_years = 400,
    civil_negative_era_year_adjustment = 399,
    civil_days_per_regular_year = 365,
    civil_days_per_era = 146097,
    civil_days_per_era_minus_one = 146096,
    civil_unix_epoch_shift_days = 719468,
    civil_month_formula_multiplier = 153,
    civil_month_formula_divisor = 5,
    civil_year_of_era_quadrennial_divisor = 1460,
    civil_year_of_era_century_divisor = 36524,
    civil_year_of_era_last_day_divisor = 146096,
};

struct convert_tz_datetime_parts {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
};

struct convert_tz_civil_date {
    int year;
    unsigned int month;
    unsigned int day;
};

static void convert_tz_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static int convert_tz_result(
    struct mylite_db *database,
    const char *datetime_text,
    size_t datetime_length,
    const char *from_tz_text,
    size_t from_tz_length,
    const char *to_tz_text,
    size_t to_tz_length,
    char **out_text,
    bool *out_is_null
);
static bool parse_datetime_parts(
    const char *text,
    size_t length,
    struct convert_tz_datetime_parts *out_parts
);
static bool parse_time_zone_offset(const char *text, size_t length, int *out_minutes);
static int append_incorrect_datetime_warning(
    struct mylite_db *database,
    const char *text,
    size_t length
);
static int format_datetime_result(
    struct mylite_db *database,
    const struct convert_tz_datetime_parts *parts,
    char **out_text
);
static bool datetime_parts_are_valid(const struct convert_tz_datetime_parts *parts);
static int days_in_month(int year, int month);
static bool is_leap_year(int year);
static int64_t days_from_civil(const struct convert_tz_civil_date *date);
static void civil_from_days(int64_t days, struct convert_tz_civil_date *out_date);
static bool parse_two_digits(const char *text, int *out_value);
static bool parse_four_digits(const char *text, int *out_value);
static bool is_ascii_digit(char byte);
static void sqlite_text_value(sqlite3_value *value, const char **out_text, size_t *out_length);

int mylite_convert_tz_value(
    struct mylite_db *database,
    const char *datetime_text,
    size_t datetime_length,
    bool datetime_is_null,
    const char *from_tz_text,
    size_t from_tz_length,
    bool from_tz_is_null,
    const char *to_tz_text,
    size_t to_tz_length,
    bool to_tz_is_null,
    char **out_text,
    bool *out_is_null
) {
    if (out_text == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_is_null = false;
    if (datetime_is_null || from_tz_is_null || to_tz_is_null) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (datetime_text == NULL || from_tz_text == NULL || to_tz_text == NULL) {
        return MYLITE_MISUSE;
    }
    return convert_tz_result(
        database,
        datetime_text,
        datetime_length,
        from_tz_text,
        from_tz_length,
        to_tz_text,
        to_tz_length,
        out_text,
        out_is_null
    );
}

int mylite_sqlite_register_convert_tz_function(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_convert_tz",
            .argument_count = 3,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = convert_tz_sqlite_callback,
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

static void convert_tz_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    const char *datetime_text = NULL;
    const char *from_tz_text = NULL;
    const char *to_tz_text = NULL;
    size_t datetime_length = 0U;
    size_t from_tz_length = 0U;
    size_t to_tz_length = 0U;
    char *result = NULL;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (database == NULL || argc != 3 || argv == NULL) {
        sqlite3_result_error(context, "MyLite CONVERT_TZ() callback failed", -1);
        return;
    }
    sqlite_text_value(argv[0], &datetime_text, &datetime_length);
    sqlite_text_value(argv[1], &from_tz_text, &from_tz_length);
    sqlite_text_value(argv[2], &to_tz_text, &to_tz_length);
    rc = mylite_convert_tz_value(
        database,
        datetime_text,
        datetime_length,
        sqlite3_value_type(argv[0]) == SQLITE_NULL,
        from_tz_text,
        from_tz_length,
        sqlite3_value_type(argv[1]) == SQLITE_NULL,
        to_tz_text,
        to_tz_length,
        sqlite3_value_type(argv[2]) == SQLITE_NULL,
        &result,
        &is_null
    );
    if (rc != MYLITE_OK) {
        free(result);
        sqlite3_result_error(context, "CONVERT_TZ() failed", -1);
        return;
    }
    if (is_null) {
        sqlite3_result_null(context);
    } else {
        sqlite3_result_text(context, result, -1, SQLITE_TRANSIENT);
    }
    free(result);
}

static int convert_tz_result(
    struct mylite_db *database,
    const char *datetime_text,
    size_t datetime_length,
    const char *from_tz_text,
    size_t from_tz_length,
    const char *to_tz_text,
    size_t to_tz_length,
    char **out_text,
    bool *out_is_null
) {
    struct convert_tz_datetime_parts parts = {0};
    int from_offset = 0;
    int to_offset = 0;
    int64_t days = 0;
    int64_t seconds = 0;
    int64_t delta_seconds = 0;
    int64_t whole_days = 0;
    int64_t day_seconds = 0;
    struct convert_tz_civil_date date = {0};

    if (!parse_datetime_parts(datetime_text, datetime_length, &parts)) {
        int rc = append_incorrect_datetime_warning(database, datetime_text, datetime_length);

        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (!parse_time_zone_offset(from_tz_text, from_tz_length, &from_offset) ||
        !parse_time_zone_offset(to_tz_text, to_tz_length, &to_offset)) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    date.year = parts.year;
    date.month = (unsigned int)parts.month;
    date.day = (unsigned int)parts.day;
    days = days_from_civil(&date);
    seconds = ((int64_t)parts.hour * convert_tz_seconds_per_hour) +
              ((int64_t)parts.minute * convert_tz_seconds_per_minute) + parts.second;
    delta_seconds = (int64_t)(to_offset - from_offset) * convert_tz_seconds_per_minute;
    seconds += delta_seconds;
    whole_days = days + (seconds / convert_tz_seconds_per_day);
    day_seconds = seconds % convert_tz_seconds_per_day;
    if (day_seconds < 0) {
        day_seconds += convert_tz_seconds_per_day;
        --whole_days;
    }

    civil_from_days(whole_days, &date);
    if (date.year < convert_tz_date_year_min || date.year > convert_tz_date_year_max) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    parts.year = date.year;
    parts.month = (int)date.month;
    parts.day = (int)date.day;
    parts.hour = (int)(day_seconds / convert_tz_seconds_per_hour);
    parts.minute =
        (int)((day_seconds % convert_tz_seconds_per_hour) / convert_tz_seconds_per_minute);
    parts.second = (int)(day_seconds % convert_tz_seconds_per_minute);
    return format_datetime_result(database, &parts, out_text);
}

static bool parse_datetime_parts(
    const char *text,
    size_t length,
    struct convert_tz_datetime_parts *out_parts
) {
    struct convert_tz_datetime_parts parts = {0};

    if (text == NULL || out_parts == NULL || length != convert_tz_datetime_length) {
        return false;
    }
    if (text[convert_tz_datetime_first_date_separator] != '-' ||
        text[convert_tz_datetime_second_date_separator] != '-' ||
        text[convert_tz_datetime_date_time_separator] != ' ' ||
        text[convert_tz_datetime_first_time_separator] != ':' ||
        text[convert_tz_datetime_second_time_separator] != ':') {
        return false;
    }
    if (!parse_four_digits(text + convert_tz_datetime_year_start, &parts.year) ||
        !parse_two_digits(text + convert_tz_datetime_month_start, &parts.month) ||
        !parse_two_digits(text + convert_tz_datetime_day_start, &parts.day) ||
        !parse_two_digits(text + convert_tz_datetime_hour_start, &parts.hour) ||
        !parse_two_digits(text + convert_tz_datetime_minute_start, &parts.minute) ||
        !parse_two_digits(text + convert_tz_datetime_second_start, &parts.second) ||
        !datetime_parts_are_valid(&parts)) {
        return false;
    }
    *out_parts = parts;
    return true;
}

static bool parse_time_zone_offset(const char *text, size_t length, int *out_minutes) {
    int hour = 0;
    int minute = 0;
    int value = 0;

    if (text == NULL || out_minutes == NULL || length != convert_tz_offset_length) {
        return false;
    }
    if ((text[convert_tz_offset_sign_index] != '+' && text[convert_tz_offset_sign_index] != '-') ||
        text[convert_tz_offset_separator_index] != ':' ||
        !parse_two_digits(text + convert_tz_offset_hour_start, &hour) ||
        !parse_two_digits(text + convert_tz_offset_minute_start, &minute) ||
        minute > convert_tz_minute_second_max) {
        return false;
    }
    value = (hour * convert_tz_minutes_per_hour) + minute;
    if (text[convert_tz_offset_sign_index] == '+') {
        if (value > convert_tz_max_positive_offset_minutes) {
            return false;
        }
    } else if (value > convert_tz_max_negative_offset_minutes) {
        return false;
    }
    *out_minutes = text[convert_tz_offset_sign_index] == '-' ? -value : value;
    return true;
}

static int append_incorrect_datetime_warning(
    struct mylite_db *database,
    const char *text,
    size_t length
) {
    char printable[convert_tz_warning_input_capacity];
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t limit = length;
    int written = 0;

    if (database == NULL) {
        return MYLITE_MISUSE;
    }
    if (limit > sizeof(printable) - 1U) {
        limit = sizeof(printable) - 1U;
    }
    for (size_t index = 0U; index < limit; ++index) {
        unsigned char byte = (unsigned char)'?';

        if (text != NULL) {
            byte = (unsigned char)text[index];
        }
        if (byte >= convert_tz_printable_ascii_min && byte <= convert_tz_printable_ascii_max) {
            printable[index] = (char)byte;
        } else {
            printable[index] = '?';
        }
    }
    printable[limit] = '\0';
    written = snprintf(message, sizeof(message), "Incorrect datetime value: '%s'", printable);
    if (written < 0 || (size_t)written >= sizeof(message)) {
        return MYLITE_NOMEM;
    }
    return mylite_diagnostics_append_warning(
        &database->diagnostics,
        mysql_warning_incorrect_datetime_value,
        "HY000",
        message
    );
}

static int format_datetime_result(
    struct mylite_db *database,
    const struct convert_tz_datetime_parts *parts,
    char **out_text
) {
    char *result = NULL;
    int written = 0;

    if (parts == NULL || out_text == NULL) {
        return MYLITE_MISUSE;
    }
    result = (char *)malloc(convert_tz_text_capacity);
    if (result == NULL) {
        if (database != NULL) {
            mylite_diagnostics_set_error(
                &database->diagnostics,
                mysql_error_internal,
                "HY000",
                "out of memory"
            );
        }
        return MYLITE_NOMEM;
    }
    written = snprintf(
        result,
        convert_tz_text_capacity,
        "%04d-%02d-%02d %02d:%02d:%02d",
        parts->year,
        parts->month,
        parts->day,
        parts->hour,
        parts->minute,
        parts->second
    );
    if (written != convert_tz_datetime_length) {
        free(result);
        mylite_diagnostics_set_error(
            &database->diagnostics,
            mysql_error_internal,
            "HY000",
            "failed to format CONVERT_TZ() result"
        );
        return MYLITE_ERROR;
    }
    *out_text = result;
    return MYLITE_OK;
}

static bool datetime_parts_are_valid(const struct convert_tz_datetime_parts *parts) {
    if (parts == NULL || parts->year < convert_tz_date_year_min ||
        parts->year > convert_tz_date_year_max || parts->month < convert_tz_date_month_min ||
        parts->month > convert_tz_date_month_max || parts->day < convert_tz_date_day_min ||
        parts->day > days_in_month(parts->year, parts->month) ||
        parts->hour < convert_tz_time_min || parts->hour > convert_tz_hour_max ||
        parts->minute < convert_tz_time_min || parts->minute > convert_tz_minute_second_max ||
        parts->second < convert_tz_time_min || parts->second > convert_tz_minute_second_max) {
        return false;
    }
    return true;
}

static int days_in_month(int year, int month) {
    static const int common_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == convert_tz_february && is_leap_year(year)) {
        return convert_tz_february_days_leap;
    }
    return common_days[month - 1];
}

static bool is_leap_year(int year) {
    if ((year % convert_tz_leap_year_cycle_full) == 0) {
        return true;
    }
    if ((year % convert_tz_leap_year_cycle_century) == 0) {
        return false;
    }
    return (year % convert_tz_leap_year_cycle_short) == 0;
}

static int64_t days_from_civil(const struct convert_tz_civil_date *date) {
    int adjusted_year = 0;
    int era = 0;
    unsigned int year_of_era = 0;
    unsigned int month_prime = 0;
    unsigned int day_of_year = 0;
    unsigned int day_of_era = 0;

    if (date == NULL) {
        return 0;
    }
    adjusted_year = date->year - (date->month < civil_march_year_start_month ? 1 : 0);
    era =
        (adjusted_year >= 0 ? adjusted_year : adjusted_year - civil_negative_era_year_adjustment) /
        civil_era_years;
    year_of_era = (unsigned int)(adjusted_year - (era * civil_era_years));
    month_prime = date->month + (date->month >= civil_march_year_start_month
                                     ? (unsigned int)-civil_march_year_start_month
                                     : (unsigned int)civil_march_year_end_adjustment);
    day_of_year =
        (((civil_month_formula_multiplier * month_prime) + 2U) / civil_month_formula_divisor) +
        date->day - 1U;
    day_of_era = (year_of_era * civil_days_per_regular_year) +
                 (year_of_era / convert_tz_leap_year_cycle_short) -
                 (year_of_era / convert_tz_leap_year_cycle_century) + day_of_year;

    return ((int64_t)era * civil_days_per_era) + (int64_t)day_of_era - civil_unix_epoch_shift_days;
}

static void civil_from_days(int64_t days, struct convert_tz_civil_date *out_date) {
    int64_t shifted = days + civil_unix_epoch_shift_days;
    int64_t era =
        (shifted >= 0 ? shifted : shifted - civil_days_per_era_minus_one) / civil_days_per_era;
    unsigned int day_of_era = (unsigned int)(shifted - (era * civil_days_per_era));
    unsigned int year_of_era = (day_of_era - (day_of_era / civil_year_of_era_quadrennial_divisor) +
                                (day_of_era / civil_year_of_era_century_divisor) -
                                (day_of_era / civil_year_of_era_last_day_divisor)) /
                               civil_days_per_regular_year;
    int year = (int)year_of_era + ((int)era * civil_era_years);
    unsigned int day_of_year = day_of_era - ((civil_days_per_regular_year * year_of_era) +
                                             (year_of_era / convert_tz_leap_year_cycle_short) -
                                             (year_of_era / convert_tz_leap_year_cycle_century));
    unsigned int month_prime =
        ((civil_month_formula_divisor * day_of_year) + 2U) / civil_month_formula_multiplier;
    unsigned int day =
        day_of_year -
        (((civil_month_formula_multiplier * month_prime) + 2U) / civil_month_formula_divisor) + 1U;
    unsigned int month = month_prime + (month_prime < civil_march_year_month_split
                                            ? (unsigned int)civil_march_year_start_month
                                            : (unsigned int)-civil_march_year_end_adjustment);

    if (out_date == NULL) {
        return;
    }
    year += month < civil_march_year_start_month ? 1 : 0;
    out_date->year = year;
    out_date->month = month;
    out_date->day = day;
}

static bool parse_two_digits(const char *text, int *out_value) {
    if (text == NULL || out_value == NULL || !is_ascii_digit(text[0]) || !is_ascii_digit(text[1])) {
        return false;
    }
    *out_value = ((int)(text[0] - '0') * convert_tz_decimal_base) + (int)(text[1] - '0');
    return true;
}

static bool parse_four_digits(const char *text, int *out_value) {
    int high = 0;
    int low = 0;

    if (!parse_two_digits(text, &high) || !parse_two_digits(text + 2, &low)) {
        return false;
    }
    *out_value = (high * (convert_tz_decimal_base * convert_tz_decimal_base)) + low;
    return true;
}

static bool is_ascii_digit(char byte) {
    return byte >= '0' && byte <= '9';
}

static void sqlite_text_value(sqlite3_value *value, const char **out_text, size_t *out_length) {
    const unsigned char *text = NULL;

    if (out_text == NULL || out_length == NULL) {
        return;
    }
    *out_text = NULL;
    *out_length = 0U;
    if (value == NULL || sqlite3_value_type(value) == SQLITE_NULL) {
        return;
    }
    text = sqlite3_value_text(value);
    *out_text = (const char *)text;
    *out_length = text == NULL ? 0U : (size_t)sqlite3_value_bytes(value);
}
