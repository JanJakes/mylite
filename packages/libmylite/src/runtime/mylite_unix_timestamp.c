#include "mylite_unix_timestamp.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_warning_incorrect_datetime_value = 1292,
    unix_timestamp_date_text_length = 10,
    unix_timestamp_datetime_text_length = 19,
    unix_timestamp_first_date_separator = 4,
    unix_timestamp_month_offset = 5,
    unix_timestamp_second_date_separator = 7,
    unix_timestamp_day_offset = 8,
    unix_timestamp_time_separator = 10,
    unix_timestamp_hour_offset = 11,
    unix_timestamp_first_time_separator = 13,
    unix_timestamp_minute_offset = 14,
    unix_timestamp_second_time_separator = 16,
    unix_timestamp_second_offset = 17,
    unix_timestamp_two_digit_count = 2,
    unix_timestamp_four_digit_count = 4,
    unix_timestamp_four_digit_year_min = 0,
    unix_timestamp_four_digit_year_max = 9999,
    unix_timestamp_month_max = 12,
    unix_timestamp_day_max = 31,
    unix_timestamp_february = 2,
    unix_timestamp_leap_day = 29,
    unix_timestamp_hour_max = 23,
    unix_timestamp_minute_second_max = 59,
    unix_timestamp_result_capacity = 32,
    unix_timestamp_digit_radix = 10,
    unix_timestamp_leap_quadrennial_year_cycle = 4,
    unix_timestamp_leap_century_year_cycle = 100,
    unix_timestamp_leap_quadricentennial_year_cycle = 400,
    unix_timestamp_days_per_common_year = 365,
    unix_timestamp_days_from_civil_epoch_shift = 719468,
    unix_timestamp_days_per_era = 146097,
    unix_timestamp_days_per_era_minus_one = 146096,
    unix_timestamp_days_per_quadrennium = 1460,
    unix_timestamp_days_per_century = 36524,
    unix_timestamp_march_based_early_month_offset = 9,
    unix_timestamp_march_based_later_month_offset = -3,
    unix_timestamp_month_formula_multiplier = 153,
    unix_timestamp_month_formula_offset = 2,
    unix_timestamp_month_formula_divisor = 5,
    unix_timestamp_march_based_month_wrap = 10,
    unix_timestamp_valid_minimum = 1,
    unix_timestamp_seconds_per_minute = 60,
    unix_timestamp_minutes_per_hour = 60,
    unix_timestamp_hours_per_day = 24,
    unix_timestamp_seconds_per_hour =
        unix_timestamp_minutes_per_hour * unix_timestamp_seconds_per_minute,
    unix_timestamp_seconds_per_day = unix_timestamp_hours_per_day * unix_timestamp_seconds_per_hour,
};

static const int64_t unix_timestamp_maximum = 32536771199LL;

struct unix_timestamp_date_parts {
    int year;
    int month;
    int day;
};

struct unix_timestamp_time_parts {
    int hour;
    int minute;
    int second;
};

struct unix_timestamp_datetime_parts {
    struct unix_timestamp_date_parts date;
    struct unix_timestamp_time_parts time;
};

struct unix_timestamp_value_source {
    const char *value;
    size_t value_length;
    enum mylite_unix_timestamp_input_kind input_kind;
};

enum unix_timestamp_parse_status {
    UNIX_TIMESTAMP_PARSE_VALID = 0,
    UNIX_TIMESTAMP_PARSE_FULL_ZERO_DATE = 1,
    UNIX_TIMESTAMP_PARSE_PARTIAL_ZERO_DATE = 2,
    UNIX_TIMESTAMP_PARSE_FRACTIONAL = 3,
    UNIX_TIMESTAMP_PARSE_INVALID = 4,
};

static void unix_timestamp_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static void from_unixtime_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static int unix_timestamp_sqlite_input_kind(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_unix_timestamp_input_kind *out_kind
);
static int unix_timestamp_sqlite_result(
    sqlite3_context *context,
    const char *value,
    size_t value_length,
    enum mylite_unix_timestamp_input_kind input_kind
);
static int from_unixtime_sqlite_result(sqlite3_context *context, int64_t seconds);
static int unix_timestamp_value_result(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    enum mylite_unix_timestamp_input_kind input_kind,
    char **out_text
);
static int from_unixtime_value_result(struct mylite_db *database, int64_t seconds, char **out_text);
static enum unix_timestamp_parse_status parse_unix_timestamp_value(
    const struct unix_timestamp_value_source *source,
    struct unix_timestamp_datetime_parts *out_datetime
);
static enum unix_timestamp_parse_status parse_string_unix_timestamp_value(
    const char *value,
    size_t value_length,
    struct unix_timestamp_datetime_parts *out_datetime
);
static enum unix_timestamp_parse_status parse_datetime_text(
    const char *value,
    size_t value_length,
    struct unix_timestamp_datetime_parts *out_datetime
);
static enum unix_timestamp_parse_status parse_date_text(
    const char *value,
    size_t value_length,
    struct unix_timestamp_date_parts *out_date
);
static bool datetime_text_has_fractional_suffix(const char *value, size_t value_length);
static bool parse_time_text(
    const char *value,
    size_t value_length,
    struct unix_timestamp_time_parts *out_time
);
static bool parse_digits(const char *text, size_t length, int *out_value);
static bool date_is_valid(const struct unix_timestamp_date_parts *date);
static bool date_is_full_zero(const struct unix_timestamp_date_parts *date);
static bool date_is_partial_zero(const struct unix_timestamp_date_parts *date);
static bool time_is_valid(const struct unix_timestamp_time_parts *time);
static int days_in_month(int year, int month);
static bool is_leap_year(int year);
static int64_t epoch_from_datetime(
    const struct unix_timestamp_datetime_parts *datetime,
    int offset_minutes
);
static struct unix_timestamp_datetime_parts datetime_from_epoch(int64_t epoch, int offset_minutes);
static int64_t days_from_civil(const struct unix_timestamp_date_parts *date);
static struct unix_timestamp_date_parts civil_from_days(int64_t days);
static int64_t floor_divide_i64(int64_t numerator, int64_t denominator);
static int copy_unix_timestamp_text(struct mylite_db *database, const char *text, char **out_text);
static int format_unix_timestamp_epoch(struct mylite_db *database, int64_t epoch, char **out_text);
static int format_from_unixtime_datetime(
    struct mylite_db *database,
    const struct unix_timestamp_datetime_parts *datetime,
    char **out_text
);
static int append_incorrect_datetime_warning(
    struct mylite_db *database,
    const char *value,
    size_t value_length
);
static void set_unix_timestamp_unsupported_error(struct mylite_db *database, const char *message);

const char *mylite_unix_timestamp_input_kind_name(enum mylite_unix_timestamp_input_kind kind) {
    switch (kind) {
    case MYLITE_UNIX_TIMESTAMP_INPUT_STRING:
        return "string";
    case MYLITE_UNIX_TIMESTAMP_INPUT_DATE:
        return "date";
    case MYLITE_UNIX_TIMESTAMP_INPUT_DATETIME:
        return "datetime";
    case MYLITE_UNIX_TIMESTAMP_INPUT_TIMESTAMP:
        return "timestamp";
    }
    return NULL;
}

bool mylite_unix_timestamp_input_kind_from_name(
    const char *name,
    size_t name_length,
    enum mylite_unix_timestamp_input_kind *out_kind
) {
    static const struct {
        const char *name;
        enum mylite_unix_timestamp_input_kind kind;
    } names[] = {
        {"string", MYLITE_UNIX_TIMESTAMP_INPUT_STRING},
        {"date", MYLITE_UNIX_TIMESTAMP_INPUT_DATE},
        {"datetime", MYLITE_UNIX_TIMESTAMP_INPUT_DATETIME},
        {"timestamp", MYLITE_UNIX_TIMESTAMP_INPUT_TIMESTAMP},
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

int mylite_unix_timestamp_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    enum mylite_unix_timestamp_input_kind input_kind,
    char **out_text,
    bool *out_is_null
) {
    if (out_text == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_is_null = false;
    if (value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    return unix_timestamp_value_result(database, value, value_length, input_kind, out_text);
}

int mylite_from_unixtime_value(
    struct mylite_db *database,
    int64_t seconds,
    bool is_null,
    char **out_text,
    bool *out_is_null
) {
    if (out_text == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_is_null = false;
    if (is_null || seconds < 0 || seconds > unix_timestamp_maximum) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    return from_unixtime_value_result(database, seconds, out_text);
}

int mylite_sqlite_register_unix_timestamp_function(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_unix_timestamp",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = unix_timestamp_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_from_unixtime",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = from_unixtime_sqlite_callback,
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

static void unix_timestamp_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    enum mylite_unix_timestamp_input_kind input_kind = MYLITE_UNIX_TIMESTAMP_INPUT_STRING;
    const unsigned char *value = NULL;
    int value_length = 0;

    if (context == NULL || argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL) {
        sqlite3_result_error(context, "invalid MyLite UNIX_TIMESTAMP callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    if (unix_timestamp_sqlite_input_kind(context, argv[1], &input_kind) != MYLITE_OK) {
        return;
    }

    value = sqlite3_value_text(argv[0]);
    value_length = sqlite3_value_bytes(argv[0]);
    if (value == NULL || value_length < 0) {
        sqlite3_result_error_nomem(context);
        return;
    }

    if (unix_timestamp_sqlite_result(
            context,
            (const char *)value,
            (size_t)value_length,
            input_kind
        ) != MYLITE_OK) {
        return;
    }
}

static void from_unixtime_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    int sqlite_type = SQLITE_NULL;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite FROM_UNIXTIME callback", -1);
        return;
    }

    sqlite_type = sqlite3_value_type(argv[0]);
    if (sqlite_type == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    if (sqlite_type != SQLITE_INTEGER) {
        sqlite3_result_error(context, "MyLite FROM_UNIXTIME expects integer seconds", -1);
        return;
    }
    (void)from_unixtime_sqlite_result(context, sqlite3_value_int64(argv[0]));
}

static int unix_timestamp_sqlite_input_kind(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_unix_timestamp_input_kind *out_kind
) {
    const unsigned char *kind_text = sqlite3_value_text(value);
    int kind_length = sqlite3_value_bytes(value);

    if (kind_text == NULL || kind_length < 0 || out_kind == NULL) {
        sqlite3_result_error_nomem(context);
        return MYLITE_NOMEM;
    }
    if (!mylite_unix_timestamp_input_kind_from_name(
            (const char *)kind_text,
            (size_t)kind_length,
            out_kind
        )) {
        sqlite3_result_error(context, "invalid MyLite UNIX_TIMESTAMP input kind", -1);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int unix_timestamp_sqlite_result(
    sqlite3_context *context,
    const char *value,
    size_t value_length,
    enum mylite_unix_timestamp_input_kind input_kind
) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    char *result = NULL;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite UNIX_TIMESTAMP owner", -1);
        return MYLITE_ERROR;
    }

    rc = mylite_unix_timestamp_value(database, value, value_length, input_kind, &result, &is_null);
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
        } else {
            sqlite3_result_error(context, "MyLite UNIX_TIMESTAMP failed", -1);
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

static int from_unixtime_sqlite_result(sqlite3_context *context, int64_t seconds) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    char *result = NULL;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite FROM_UNIXTIME owner", -1);
        return MYLITE_ERROR;
    }

    rc = mylite_from_unixtime_value(database, seconds, false, &result, &is_null);
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
        } else {
            sqlite3_result_error(context, "MyLite FROM_UNIXTIME failed", -1);
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

static int unix_timestamp_value_result(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    enum mylite_unix_timestamp_input_kind input_kind,
    char **out_text
) {
    struct unix_timestamp_datetime_parts datetime = {0};
    const struct unix_timestamp_value_source source = {
        .value = value,
        .value_length = value_length,
        .input_kind = input_kind,
    };
    enum unix_timestamp_parse_status status = parse_unix_timestamp_value(&source, &datetime);
    int offset_minutes = 0;
    int64_t epoch = 0;
    int rc = MYLITE_OK;

    if (status == UNIX_TIMESTAMP_PARSE_FRACTIONAL) {
        set_unix_timestamp_unsupported_error(
            database,
            "UNIX_TIMESTAMP() does not yet support fractional seconds"
        );
        return MYLITE_ERROR;
    }
    if (status == UNIX_TIMESTAMP_PARSE_FULL_ZERO_DATE) {
        rc = append_incorrect_datetime_warning(database, value, value_length);
        if (rc != MYLITE_OK) {
            return rc;
        }
        return copy_unix_timestamp_text(database, "0", out_text);
    }
    if (status == UNIX_TIMESTAMP_PARSE_PARTIAL_ZERO_DATE) {
        return copy_unix_timestamp_text(database, "0", out_text);
    }
    if (status == UNIX_TIMESTAMP_PARSE_INVALID) {
        rc = append_incorrect_datetime_warning(database, value, value_length);
        if (rc != MYLITE_OK) {
            return rc;
        }
        return copy_unix_timestamp_text(database, "0.000000", out_text);
    }

    if (input_kind != MYLITE_UNIX_TIMESTAMP_INPUT_TIMESTAMP) {
        offset_minutes = database == NULL ? 0 : database->session.time_zone_offset_minutes;
    }
    epoch = epoch_from_datetime(&datetime, offset_minutes);
    return format_unix_timestamp_epoch(database, epoch, out_text);
}

static int from_unixtime_value_result(
    struct mylite_db *database,
    int64_t seconds,
    char **out_text
) {
    struct unix_timestamp_datetime_parts datetime = datetime_from_epoch(
        seconds,
        database == NULL ? 0 : database->session.time_zone_offset_minutes
    );

    return format_from_unixtime_datetime(database, &datetime, out_text);
}

static enum unix_timestamp_parse_status parse_unix_timestamp_value(
    const struct unix_timestamp_value_source *source,
    struct unix_timestamp_datetime_parts *out_datetime
) {
    if (out_datetime == NULL) {
        return UNIX_TIMESTAMP_PARSE_INVALID;
    }
    *out_datetime = (struct unix_timestamp_datetime_parts){0};
    if (source == NULL || source->value == NULL) {
        return UNIX_TIMESTAMP_PARSE_INVALID;
    }

    switch (source->input_kind) {
    case MYLITE_UNIX_TIMESTAMP_INPUT_STRING:
        return parse_string_unix_timestamp_value(source->value, source->value_length, out_datetime);
    case MYLITE_UNIX_TIMESTAMP_INPUT_DATE: {
        enum unix_timestamp_parse_status status =
            parse_date_text(source->value, source->value_length, &out_datetime->date);

        out_datetime->time = (struct unix_timestamp_time_parts){0};
        return status;
    }
    case MYLITE_UNIX_TIMESTAMP_INPUT_DATETIME:
    case MYLITE_UNIX_TIMESTAMP_INPUT_TIMESTAMP:
        return parse_datetime_text(source->value, source->value_length, out_datetime);
    }
    return UNIX_TIMESTAMP_PARSE_INVALID;
}

static enum unix_timestamp_parse_status parse_string_unix_timestamp_value(
    const char *value,
    size_t value_length,
    struct unix_timestamp_datetime_parts *out_datetime
) {
    enum unix_timestamp_parse_status status = UNIX_TIMESTAMP_PARSE_INVALID;

    if (datetime_text_has_fractional_suffix(value, value_length)) {
        return UNIX_TIMESTAMP_PARSE_FRACTIONAL;
    }
    status = parse_datetime_text(value, value_length, out_datetime);
    if (status != UNIX_TIMESTAMP_PARSE_INVALID) {
        return status;
    }
    status = parse_date_text(value, value_length, &out_datetime->date);
    if (status == UNIX_TIMESTAMP_PARSE_VALID) {
        out_datetime->time = (struct unix_timestamp_time_parts){0};
    }
    return status;
}

static enum unix_timestamp_parse_status parse_datetime_text(
    const char *value,
    size_t value_length,
    struct unix_timestamp_datetime_parts *out_datetime
) {
    enum unix_timestamp_parse_status date_status = UNIX_TIMESTAMP_PARSE_INVALID;

    if (value == NULL || out_datetime == NULL ||
        value_length != unix_timestamp_datetime_text_length ||
        value[unix_timestamp_time_separator] != ' ') {
        return UNIX_TIMESTAMP_PARSE_INVALID;
    }
    date_status = parse_date_text(value, unix_timestamp_date_text_length, &out_datetime->date);
    if (date_status != UNIX_TIMESTAMP_PARSE_VALID) {
        return date_status;
    }
    if (!parse_time_text(
            value + unix_timestamp_hour_offset,
            unix_timestamp_datetime_text_length - unix_timestamp_hour_offset,
            &out_datetime->time
        )) {
        return UNIX_TIMESTAMP_PARSE_INVALID;
    }
    return UNIX_TIMESTAMP_PARSE_VALID;
}

static enum unix_timestamp_parse_status parse_date_text(
    const char *value,
    size_t value_length,
    struct unix_timestamp_date_parts *out_date
) {
    struct unix_timestamp_date_parts date = {0};

    if (value == NULL || out_date == NULL || value_length != unix_timestamp_date_text_length ||
        value[unix_timestamp_first_date_separator] != '-' ||
        value[unix_timestamp_second_date_separator] != '-') {
        return UNIX_TIMESTAMP_PARSE_INVALID;
    }
    if (!parse_digits(value, unix_timestamp_four_digit_count, &date.year) ||
        !parse_digits(
            value + unix_timestamp_month_offset,
            unix_timestamp_two_digit_count,
            &date.month
        ) ||
        !parse_digits(
            value + unix_timestamp_day_offset,
            unix_timestamp_two_digit_count,
            &date.day
        )) {
        return UNIX_TIMESTAMP_PARSE_INVALID;
    }

    *out_date = date;
    if (date_is_full_zero(&date)) {
        return UNIX_TIMESTAMP_PARSE_FULL_ZERO_DATE;
    }
    if (date_is_partial_zero(&date)) {
        return UNIX_TIMESTAMP_PARSE_PARTIAL_ZERO_DATE;
    }
    if (date_is_valid(&date)) {
        return UNIX_TIMESTAMP_PARSE_VALID;
    }
    return UNIX_TIMESTAMP_PARSE_INVALID;
}

static bool datetime_text_has_fractional_suffix(const char *value, size_t value_length) {
    if (value == NULL || value_length <= unix_timestamp_datetime_text_length ||
        value[unix_timestamp_datetime_text_length] != '.') {
        return false;
    }
    for (size_t index = unix_timestamp_datetime_text_length + 1U; index < value_length; ++index) {
        if (value[index] < '0' || value[index] > '9') {
            return false;
        }
    }
    return value_length > unix_timestamp_datetime_text_length + 1U;
}

static bool parse_time_text(
    const char *value,
    size_t value_length,
    struct unix_timestamp_time_parts *out_time
) {
    struct unix_timestamp_time_parts time = {0};

    if (value == NULL || out_time == NULL ||
        value_length != unix_timestamp_datetime_text_length - unix_timestamp_hour_offset ||
        value[unix_timestamp_first_time_separator - unix_timestamp_hour_offset] != ':' ||
        value[unix_timestamp_second_time_separator - unix_timestamp_hour_offset] != ':') {
        return false;
    }
    if (!parse_digits(value, unix_timestamp_two_digit_count, &time.hour) ||
        !parse_digits(
            value + unix_timestamp_minute_offset - unix_timestamp_hour_offset,
            unix_timestamp_two_digit_count,
            &time.minute
        ) ||
        !parse_digits(
            value + unix_timestamp_second_offset - unix_timestamp_hour_offset,
            unix_timestamp_two_digit_count,
            &time.second
        )) {
        return false;
    }
    if (!time_is_valid(&time)) {
        return false;
    }
    *out_time = time;
    return true;
}

static bool parse_digits(const char *text, size_t length, int *out_value) {
    int value = 0;

    if (text == NULL || out_value == NULL || length == 0U) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
        value = (value * unix_timestamp_digit_radix) + (text[index] - '0');
    }
    *out_value = value;
    return true;
}

static bool date_is_valid(const struct unix_timestamp_date_parts *date) {
    if (date == NULL || date->month < 1 || date->month > unix_timestamp_month_max ||
        date->day < 1 || date->day > unix_timestamp_day_max) {
        return false;
    }
    return date->day <= days_in_month(date->year, date->month);
}

static bool date_is_full_zero(const struct unix_timestamp_date_parts *date) {
    if (date == NULL) {
        return false;
    }
    return (date->year == 0 && date->month == 0 && date->day == 0) != 0;
}

static bool date_is_partial_zero(const struct unix_timestamp_date_parts *date) {
    if (date == NULL || date_is_full_zero(date)) {
        return false;
    }
    return (date->year == 0 || date->month == 0 || date->day == 0) != 0;
}

static bool time_is_valid(const struct unix_timestamp_time_parts *time) {
    if (time == NULL) {
        return false;
    }
    return (time->hour >= 0 && time->hour <= unix_timestamp_hour_max && time->minute >= 0 &&
            time->minute <= unix_timestamp_minute_second_max && time->second >= 0 &&
            time->second <= unix_timestamp_minute_second_max) != 0;
}

static int days_in_month(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == unix_timestamp_february && is_leap_year(year)) {
        return unix_timestamp_leap_day;
    }
    if (month < 1 || month > unix_timestamp_month_max) {
        return 0;
    }
    return days[month - 1];
}

static bool is_leap_year(int year) {
    return ((year % unix_timestamp_leap_quadrennial_year_cycle == 0 &&
             year % unix_timestamp_leap_century_year_cycle != 0) ||
            year % unix_timestamp_leap_quadricentennial_year_cycle == 0) != 0;
}

static int64_t epoch_from_datetime(
    const struct unix_timestamp_datetime_parts *datetime,
    int offset_minutes
) {
    int64_t days = days_from_civil(datetime == NULL ? NULL : &datetime->date);
    int64_t seconds = days * unix_timestamp_hours_per_day * unix_timestamp_minutes_per_hour *
                      unix_timestamp_seconds_per_minute;

    if (datetime != NULL) {
        seconds += (int64_t)datetime->time.hour * unix_timestamp_minutes_per_hour *
                   unix_timestamp_seconds_per_minute;
        seconds += (int64_t)datetime->time.minute * unix_timestamp_seconds_per_minute;
        seconds += datetime->time.second;
    }
    seconds -= (int64_t)offset_minutes * unix_timestamp_seconds_per_minute;
    return seconds;
}

static struct unix_timestamp_datetime_parts datetime_from_epoch(int64_t epoch, int offset_minutes) {
    int64_t local_epoch = epoch + ((int64_t)offset_minutes * unix_timestamp_seconds_per_minute);
    int64_t seconds_per_day = unix_timestamp_seconds_per_day;
    int64_t days = floor_divide_i64(local_epoch, seconds_per_day);
    int64_t seconds_of_day = local_epoch - (days * seconds_per_day);
    struct unix_timestamp_datetime_parts datetime = {0};

    datetime.date = civil_from_days(days);
    datetime.time.hour = (int)(seconds_of_day / unix_timestamp_seconds_per_hour);
    seconds_of_day %= unix_timestamp_seconds_per_hour;
    datetime.time.minute = (int)(seconds_of_day / unix_timestamp_seconds_per_minute);
    datetime.time.second = (int)(seconds_of_day % unix_timestamp_seconds_per_minute);
    return datetime;
}

static int64_t days_from_civil(const struct unix_timestamp_date_parts *date) {
    int year = date == NULL ? 0 : date->year;
    int month = date == NULL ? 0 : date->month;
    int day = date == NULL ? 0 : date->day;
    int adjusted_year = year - (month <= 2 ? 1 : 0);
    int era = (adjusted_year >= 0
                   ? adjusted_year
                   : adjusted_year - (unix_timestamp_leap_quadricentennial_year_cycle - 1)) /
              unix_timestamp_leap_quadricentennial_year_cycle;
    unsigned int year_of_era =
        (unsigned int)(adjusted_year - (era * unix_timestamp_leap_quadricentennial_year_cycle));
    unsigned int month_term =
        (unsigned int)(month + (month > unix_timestamp_february
                                    ? unix_timestamp_march_based_later_month_offset
                                    : unix_timestamp_march_based_early_month_offset));
    unsigned int day_of_year = (((unix_timestamp_month_formula_multiplier * month_term) +
                                 unix_timestamp_month_formula_offset) /
                                unix_timestamp_month_formula_divisor) +
                               (unsigned int)day - 1U;
    unsigned int day_of_era = (year_of_era * unix_timestamp_days_per_common_year) +
                              (year_of_era / unix_timestamp_leap_quadrennial_year_cycle) -
                              (year_of_era / unix_timestamp_leap_century_year_cycle) + day_of_year;

    return ((int64_t)era * unix_timestamp_days_per_era) + (int64_t)day_of_era -
           unix_timestamp_days_from_civil_epoch_shift;
}

static struct unix_timestamp_date_parts civil_from_days(int64_t days) {
    int64_t shifted_days = days + unix_timestamp_days_from_civil_epoch_shift;
    int64_t era = floor_divide_i64(shifted_days, unix_timestamp_days_per_era);
    uint64_t day_of_era = (uint64_t)(shifted_days - (era * unix_timestamp_days_per_era));
    uint64_t year_of_era = (day_of_era - (day_of_era / unix_timestamp_days_per_quadrennium) +
                            (day_of_era / unix_timestamp_days_per_century) -
                            (day_of_era / unix_timestamp_days_per_era_minus_one)) /
                           unix_timestamp_days_per_common_year;
    int64_t year = (era * unix_timestamp_leap_quadricentennial_year_cycle) + (int64_t)year_of_era;
    uint64_t day_of_year =
        day_of_era - ((year_of_era * unix_timestamp_days_per_common_year) +
                      (year_of_era / unix_timestamp_leap_quadrennial_year_cycle) -
                      (year_of_era / unix_timestamp_leap_century_year_cycle));
    uint64_t month_part = ((unix_timestamp_month_formula_divisor * day_of_year) +
                           unix_timestamp_month_formula_offset) /
                          unix_timestamp_month_formula_multiplier;
    int day = (int)(day_of_year -
                    (((unix_timestamp_month_formula_multiplier * month_part) +
                      unix_timestamp_month_formula_offset) /
                     unix_timestamp_month_formula_divisor) +
                    1U);
    int month = (int)month_part + (month_part < unix_timestamp_march_based_month_wrap
                                       ? -unix_timestamp_march_based_later_month_offset
                                       : -unix_timestamp_march_based_early_month_offset);

    year += month <= unix_timestamp_february ? 1 : 0;
    return (struct unix_timestamp_date_parts){
        .year = (int)year,
        .month = month,
        .day = day,
    };
}

static int64_t floor_divide_i64(int64_t numerator, int64_t denominator) {
    int64_t quotient = numerator / denominator;
    int64_t remainder = numerator % denominator;

    if (remainder != 0 && ((remainder < 0) != (denominator < 0))) {
        --quotient;
    }
    return quotient;
}

static int copy_unix_timestamp_text(struct mylite_db *database, const char *text, char **out_text) {
    size_t length = text == NULL ? 0U : strlen(text);
    char *copy = NULL;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    copy = (char *)malloc(length + 1U);
    if (copy == NULL) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_NOMEM,
            "HY001",
            "out of memory"
        );
        return MYLITE_NOMEM;
    }
    if (length != 0U) {
        memcpy(copy, text, length);
    }
    copy[length] = '\0';
    *out_text = copy;
    return MYLITE_OK;
}

static int format_unix_timestamp_epoch(struct mylite_db *database, int64_t epoch, char **out_text) {
    char buffer[unix_timestamp_result_capacity];
    int64_t result = epoch;
    int written = 0;

    if (epoch < unix_timestamp_valid_minimum || epoch > unix_timestamp_maximum) {
        result = 0;
    }
    written = snprintf(buffer, sizeof(buffer), "%" PRId64, result);
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        set_unix_timestamp_unsupported_error(database, "failed to format UNIX_TIMESTAMP() value");
        return MYLITE_ERROR;
    }
    return copy_unix_timestamp_text(database, buffer, out_text);
}

static int format_from_unixtime_datetime(
    struct mylite_db *database,
    const struct unix_timestamp_datetime_parts *datetime,
    char **out_text
) {
    char buffer[unix_timestamp_result_capacity];
    int written = 0;

    if (datetime == NULL || datetime->date.year < unix_timestamp_four_digit_year_min ||
        datetime->date.year > unix_timestamp_four_digit_year_max) {
        set_unix_timestamp_unsupported_error(database, "failed to format FROM_UNIXTIME() value");
        return MYLITE_ERROR;
    }

    written = snprintf(
        buffer,
        sizeof(buffer),
        "%04d-%02d-%02d %02d:%02d:%02d",
        datetime->date.year,
        datetime->date.month,
        datetime->date.day,
        datetime->time.hour,
        datetime->time.minute,
        datetime->time.second
    );
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        set_unix_timestamp_unsupported_error(database, "failed to format FROM_UNIXTIME() value");
        return MYLITE_ERROR;
    }
    return copy_unix_timestamp_text(database, buffer, out_text);
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

static void set_unix_timestamp_unsupported_error(struct mylite_db *database, const char *message) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_parse,
        "42000",
        message
    );
}
