#include "mylite_temporal_arithmetic.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    temporal_date_text_length = 10,
    temporal_date_year_text_offset = 0,
    temporal_date_year_text_length = 4,
    temporal_date_first_separator_offset = 4,
    temporal_date_month_text_offset = 5,
    temporal_date_month_text_length = 2,
    temporal_date_second_separator_offset = 7,
    temporal_date_day_text_offset = 8,
    temporal_date_day_text_length = 2,
    temporal_datetime_text_length = 19,
    temporal_datetime_date_time_separator_offset = 10,
    temporal_datetime_hour_text_offset = 11,
    temporal_datetime_hour_text_length = 2,
    temporal_datetime_first_time_separator_offset = 13,
    temporal_datetime_minute_text_offset = 14,
    temporal_datetime_minute_text_length = 2,
    temporal_datetime_second_time_separator_offset = 16,
    temporal_datetime_second_text_offset = 17,
    temporal_datetime_second_text_length = 2,
    temporal_datetime_minimum_hour = 0,
    temporal_datetime_maximum_hour = 23,
    temporal_datetime_minimum_minute_or_second = 0,
    temporal_datetime_maximum_minute_or_second = 59,
    temporal_datetime_hours_per_day = 24,
    temporal_decimal_base = 10,
    temporal_date_minimum_year = 1000,
    temporal_date_maximum_year = 9999,
    temporal_date_first_month = 1,
    temporal_date_first_day = 1,
    temporal_date_february = 2,
    temporal_date_months_per_year = 12,
    temporal_date_maximum_day_field = 31,
    temporal_date_leap_day = 29,
    temporal_date_leap_year_quadricentennial = 400,
    temporal_date_leap_year_century = 100,
    temporal_date_leap_year_quadrennial = 4,
    temporal_march_year_shift_month = temporal_date_february,
    temporal_march_based_month_switch = 10,
    temporal_month_shift = 3,
    temporal_march_based_months_after_february = 9,
    temporal_month_scale = 153,
    temporal_month_bias = 2,
    temporal_month_divisor = 5,
    temporal_days_per_non_leap_year = 365,
    temporal_era_year_offset = temporal_date_leap_year_quadricentennial - 1,
    temporal_leap_cycle_four_year_days = 1460,
    temporal_leap_cycle_century_days = 36524,
    temporal_days_per_era = 146097,
    temporal_days_per_era_offset = temporal_days_per_era - 1,
    temporal_unix_epoch_day_offset = 719468,
    temporal_time_second_per_minute = 60,
    temporal_time_second_per_hour = 3600,
};

static bool date_text_is_canonical_valid(const char *text, size_t text_length);
static bool datetime_text_is_canonical_valid(const char *text, size_t text_length);
static bool date_text_has_canonical_shape(const char *text, size_t text_length);
static bool datetime_text_has_canonical_shape(const char *text, size_t text_length);
static bool date_component_text_to_u32(const char *text, size_t length, uint32_t *out_value);
static bool date_year_month_day_valid(uint32_t year, uint32_t month, uint32_t day);
static bool datetime_time_components_valid(uint32_t hour, uint32_t minute, uint32_t second);
static bool date_year_is_leap(uint32_t year);
static uint32_t days_in_month(int64_t year, uint32_t month);

bool mylite_temporal_arithmetic_parse_datetime_text(
    const char *text,
    size_t text_length,
    struct mylite_temporal_datetime_parts *out_datetime
) {
    uint32_t year = 0U;

    if (out_datetime == NULL) {
        return false;
    }
    *out_datetime = (struct mylite_temporal_datetime_parts){0};
    if (date_text_is_canonical_valid(text, text_length)) {
        if (!date_component_text_to_u32(
                text + temporal_date_year_text_offset,
                temporal_date_year_text_length,
                &year
            ) ||
            !date_component_text_to_u32(
                text + temporal_date_month_text_offset,
                temporal_date_month_text_length,
                &out_datetime->month
            ) ||
            !date_component_text_to_u32(
                text + temporal_date_day_text_offset,
                temporal_date_day_text_length,
                &out_datetime->day
            )) {
            return false;
        }
        out_datetime->year = (int64_t)year;
        return true;
    }
    if (!datetime_text_is_canonical_valid(text, text_length)) {
        return false;
    }
    if (!date_component_text_to_u32(
            text + temporal_date_year_text_offset,
            temporal_date_year_text_length,
            &year
        ) ||
        !date_component_text_to_u32(
            text + temporal_date_month_text_offset,
            temporal_date_month_text_length,
            &out_datetime->month
        ) ||
        !date_component_text_to_u32(
            text + temporal_date_day_text_offset,
            temporal_date_day_text_length,
            &out_datetime->day
        ) ||
        !date_component_text_to_u32(
            text + temporal_datetime_hour_text_offset,
            temporal_datetime_hour_text_length,
            &out_datetime->hour
        ) ||
        !date_component_text_to_u32(
            text + temporal_datetime_minute_text_offset,
            temporal_datetime_minute_text_length,
            &out_datetime->minute
        ) ||
        !date_component_text_to_u32(
            text + temporal_datetime_second_text_offset,
            temporal_datetime_second_text_length,
            &out_datetime->second
        )) {
        return false;
    }
    out_datetime->year = (int64_t)year;
    return true;
}

bool mylite_temporal_arithmetic_checked_add_int64(int64_t left, int64_t right, int64_t *out_value) {
    if (out_value == NULL) {
        return false;
    }
    if ((right > 0 && left > INT64_MAX - right) || (right < 0 && left < INT64_MIN - right)) {
        return false;
    }
    *out_value = left + right;
    return true;
}

bool mylite_temporal_arithmetic_checked_multiply_int64(
    int64_t left,
    int64_t right,
    int64_t *out_value
) {
    if (out_value == NULL) {
        return false;
    }
    if (left == 0 || right == 0) {
        *out_value = 0;
        return true;
    }
    if (left == -1 && right == INT64_MIN) {
        return false;
    }
    if (right == -1 && left == INT64_MIN) {
        return false;
    }
    if (left > 0) {
        if ((right > 0 && left > INT64_MAX / right) || (right < 0 && right < INT64_MIN / left)) {
            return false;
        }
    } else if ((right > 0 && left < INT64_MIN / right) || (right < 0 && left < INT64_MAX / right)) {
        return false;
    }
    *out_value = left * right;
    return true;
}

bool mylite_temporal_arithmetic_add_calendar_months(
    const struct mylite_temporal_datetime_parts *input,
    int64_t interval_months,
    struct mylite_temporal_datetime_parts *out_datetime
) {
    int64_t zero_based_month = 0;
    int64_t result_month = 0;
    uint32_t day_maximum = 0U;

    if (input == NULL || out_datetime == NULL) {
        return false;
    }
    *out_datetime = *input;
    if (!mylite_temporal_arithmetic_checked_add_int64(
            (input->year * temporal_date_months_per_year) + (int64_t)input->month - 1,
            interval_months,
            &zero_based_month
        )) {
        return false;
    }
    out_datetime->year = zero_based_month / temporal_date_months_per_year;
    result_month = zero_based_month % temporal_date_months_per_year;
    if (result_month < 0) {
        result_month += temporal_date_months_per_year;
        --out_datetime->year;
    }
    out_datetime->month = (uint32_t)result_month + 1U;
    if (out_datetime->year < temporal_date_minimum_year ||
        out_datetime->year > temporal_date_maximum_year) {
        return false;
    }
    day_maximum = days_in_month(out_datetime->year, out_datetime->month);
    if (out_datetime->day > day_maximum) {
        out_datetime->day = day_maximum;
    }
    return true;
}

int64_t mylite_temporal_arithmetic_seconds_per_day(void) {
    return (int64_t)temporal_datetime_hours_per_day * (int64_t)temporal_time_second_per_hour;
}

int64_t mylite_temporal_arithmetic_days_from_datetime(
    const struct mylite_temporal_datetime_parts *datetime
) {
    uint32_t month_prime = 0U;
    uint32_t year_of_era = 0U;
    uint32_t day_of_year = 0U;
    uint32_t day_of_era = 0U;
    int64_t era = 0;
    int64_t year = 0;

    if (datetime == NULL) {
        return 0;
    }
    year = datetime->year;
    year -= datetime->month <= temporal_march_year_shift_month ? 1 : 0;
    era = (year >= 0 ? year : year - temporal_era_year_offset) /
          temporal_date_leap_year_quadricentennial;
    year_of_era = (uint32_t)(year - (era * temporal_date_leap_year_quadricentennial));
    month_prime = datetime->month > temporal_march_year_shift_month
                      ? datetime->month - temporal_month_shift
                      : datetime->month + temporal_march_based_months_after_february;
    day_of_year =
        (((temporal_month_scale * month_prime) + temporal_month_bias) / temporal_month_divisor) +
        datetime->day - 1U;
    day_of_era = (year_of_era * temporal_days_per_non_leap_year) +
                 (year_of_era / temporal_date_leap_year_quadrennial) -
                 (year_of_era / temporal_date_leap_year_century) + day_of_year;

    return (era * temporal_days_per_era) + (int64_t)day_of_era - temporal_unix_epoch_day_offset;
}

void mylite_temporal_arithmetic_civil_from_days(
    int64_t days,
    struct mylite_temporal_datetime_parts *out_datetime
) {
    uint32_t day_of_era = 0U;
    uint32_t year_of_era = 0U;
    uint32_t day_of_year = 0U;
    uint32_t month_prime = 0U;
    int64_t era = 0;
    int64_t year = 0;

    if (out_datetime == NULL) {
        return;
    }
    days += temporal_unix_epoch_day_offset;
    era = (days >= 0 ? days : days - temporal_days_per_era_offset) / temporal_days_per_era;
    day_of_era = (uint32_t)(days - (era * temporal_days_per_era));
    year_of_era = (day_of_era - (day_of_era / temporal_leap_cycle_four_year_days) +
                   (day_of_era / temporal_leap_cycle_century_days) -
                   (day_of_era / temporal_days_per_era_offset)) /
                  temporal_days_per_non_leap_year;
    year = (int64_t)year_of_era + (era * temporal_date_leap_year_quadricentennial);
    day_of_year = day_of_era - ((temporal_days_per_non_leap_year * year_of_era) +
                                (year_of_era / temporal_date_leap_year_quadrennial) -
                                (year_of_era / temporal_date_leap_year_century));
    month_prime =
        ((temporal_month_divisor * day_of_year) + temporal_month_bias) / temporal_month_scale;

    out_datetime->day =
        day_of_year -
        (((temporal_month_scale * month_prime) + temporal_month_bias) / temporal_month_divisor) +
        1U;
    out_datetime->month = month_prime < temporal_march_based_month_switch
                              ? month_prime + temporal_month_shift
                              : month_prime - temporal_march_based_months_after_february;
    out_datetime->year = year + (out_datetime->month <= temporal_march_year_shift_month ? 1 : 0);
}

struct mylite_temporal_day_second mylite_temporal_arithmetic_floor_divmod_seconds(
    int64_t total_seconds
) {
    const int64_t seconds_per_day = mylite_temporal_arithmetic_seconds_per_day();
    struct mylite_temporal_day_second result = {0};

    result.days = total_seconds / seconds_per_day;
    result.day_seconds = total_seconds % seconds_per_day;

    if (result.day_seconds < 0) {
        result.day_seconds += seconds_per_day;
        --result.days;
    }
    return result;
}

static bool date_text_is_canonical_valid(const char *text, size_t text_length) {
    uint32_t year = 0U;
    uint32_t month = 0U;
    uint32_t day = 0U;

    return (date_text_has_canonical_shape(text, text_length) &&
            date_component_text_to_u32(
                text + temporal_date_year_text_offset,
                temporal_date_year_text_length,
                &year
            ) &&
            date_component_text_to_u32(
                text + temporal_date_month_text_offset,
                temporal_date_month_text_length,
                &month
            ) &&
            date_component_text_to_u32(
                text + temporal_date_day_text_offset,
                temporal_date_day_text_length,
                &day
            ) &&
            date_year_month_day_valid(year, month, day)) != 0;
}

static bool datetime_text_is_canonical_valid(const char *text, size_t text_length) {
    uint32_t hour = 0U;
    uint32_t minute = 0U;
    uint32_t second = 0U;

    return (datetime_text_has_canonical_shape(text, text_length) &&
            date_text_is_canonical_valid(text, temporal_date_text_length) &&
            date_component_text_to_u32(
                text + temporal_datetime_hour_text_offset,
                temporal_datetime_hour_text_length,
                &hour
            ) &&
            date_component_text_to_u32(
                text + temporal_datetime_minute_text_offset,
                temporal_datetime_minute_text_length,
                &minute
            ) &&
            date_component_text_to_u32(
                text + temporal_datetime_second_text_offset,
                temporal_datetime_second_text_length,
                &second
            ) &&
            datetime_time_components_valid(hour, minute, second)) != 0;
}

static bool date_text_has_canonical_shape(const char *text, size_t text_length) {
    if (text == NULL || text_length != temporal_date_text_length) {
        return false;
    }
    return (text[temporal_date_first_separator_offset] == '-' &&
            text[temporal_date_second_separator_offset] == '-') != 0;
}

static bool datetime_text_has_canonical_shape(const char *text, size_t text_length) {
    if (text == NULL || text_length != temporal_datetime_text_length) {
        return false;
    }
    return (text[temporal_datetime_date_time_separator_offset] == ' ' &&
            text[temporal_datetime_first_time_separator_offset] == ':' &&
            text[temporal_datetime_second_time_separator_offset] == ':') != 0;
}

static bool date_component_text_to_u32(const char *text, size_t length, uint32_t *out_value) {
    uint32_t value = 0U;

    if (text == NULL || out_value == NULL || length == 0U) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (byte < '0' || byte > '9') {
            return false;
        }
        value = (value * temporal_decimal_base) + (uint32_t)(byte - '0');
    }
    *out_value = value;
    return true;
}

static bool date_year_month_day_valid(uint32_t year, uint32_t month, uint32_t day) {
    uint32_t day_maximum = 0U;

    if (year < temporal_date_minimum_year || year > temporal_date_maximum_year ||
        month < temporal_date_first_month || month > temporal_date_months_per_year ||
        day < temporal_date_first_day || day > temporal_date_maximum_day_field) {
        return false;
    }
    day_maximum = days_in_month((int64_t)year, month);
    return day <= day_maximum;
}

static bool datetime_time_components_valid(uint32_t hour, uint32_t minute, uint32_t second) {
    return (hour <= temporal_datetime_maximum_hour &&
            minute <= temporal_datetime_maximum_minute_or_second &&
            second <= temporal_datetime_maximum_minute_or_second) != 0;
}

static bool date_year_is_leap(uint32_t year) {
    return ((year % temporal_date_leap_year_quadricentennial) == 0U ||
            ((year % temporal_date_leap_year_quadrennial) == 0U &&
             (year % temporal_date_leap_year_century) != 0U)) != 0;
}

static uint32_t days_in_month(int64_t year, uint32_t month) {
    static const uint32_t month_days[] = {
        31U,
        28U,
        31U,
        30U,
        31U,
        30U,
        31U,
        31U,
        30U,
        31U,
        30U,
        31U,
    };

    if (month < temporal_date_first_month || month > temporal_date_months_per_year) {
        return 0U;
    }
    if (month == temporal_date_february && year >= 0 && year <= UINT32_MAX &&
        date_year_is_leap((uint32_t)year)) {
        return temporal_date_leap_day;
    }
    return month_days[month - 1U];
}
