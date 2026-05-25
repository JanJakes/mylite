#include "mylite_temporal_constructor.h"

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
    mysql_warning_datetime_field_overflow = 1441,
    mysql_warning_incorrect_time_value = 1292,
    date_text_length = 10,
    time_text_max_length = 10,
    integer_result_buffer_capacity = 32,
    date_month_max = 12,
    date_february = 2,
    date_leap_day = 29,
    time_hour_max = 838,
    time_minute_second_max = 59,
    leap_quadrennial_year_cycle = 4,
    leap_century_year_cycle = 100,
    leap_quadricentennial_year_cycle = 400,
    days_per_common_year = 365,
    two_digit_year_cutoff = 70,
    two_digit_year_upper_bound = 100,
    two_digit_year_low_base = 2000,
    two_digit_year_high_base = 1900,
    date_year_max = 9999,
};

static const int64_t from_days_first_valid_day_number = 366;
static const int64_t from_days_last_valid_day_number = 3652424;
static const int64_t from_days_first_zero_after_overflow_day_number = 3652500;

struct temporal_constructor_date_parts {
    int year;
    int month;
    int day;
};

struct temporal_constructor_time_parts {
    bool negative;
    int hour;
    int minute;
    int second;
};

struct temporal_constructor_makedate_arguments {
    int64_t year;
    int64_t day_of_year;
};

static void from_days_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void makedate_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void maketime_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static int temporal_constructor_sqlite_result(
    sqlite3_context *context,
    char *result,
    bool is_null,
    int rc
);
static int from_days_result(
    struct mylite_db *database,
    int64_t day_number,
    char **out_text,
    bool *out_is_null
);
static int makedate_result(
    struct mylite_db *database,
    const struct temporal_constructor_makedate_arguments *arguments,
    char **out_text,
    bool *out_is_null
);
static int maketime_result(
    struct mylite_db *database,
    int64_t hour,
    int64_t minute,
    int64_t second,
    char **out_text,
    bool *out_is_null
);
static int copy_static_result(struct mylite_db *database, const char *value, char **out_text);
static int format_date_result(
    struct mylite_db *database,
    const struct temporal_constructor_date_parts *parts,
    char **out_text
);
static int format_time_result(
    struct mylite_db *database,
    const struct temporal_constructor_time_parts *parts,
    char **out_text
);
static int append_from_days_overflow_warning(struct mylite_db *database);
static int append_maketime_truncation_warning(
    struct mylite_db *database,
    int64_t hour,
    int64_t minute,
    int64_t second
);
static int64_t day_number_for_date(const struct temporal_constructor_date_parts *parts);
static void date_from_day_number(
    int64_t day_number,
    struct temporal_constructor_date_parts *out_parts
);
static int convert_makedate_year(int64_t year);
static bool is_leap_year(int year);
static int days_in_month(int year, int month);
static int day_of_year(const struct temporal_constructor_date_parts *parts);
static int set_temporal_constructor_nomem_error(struct mylite_db *database);

int mylite_from_days_value(
    struct mylite_db *database,
    int64_t day_number,
    bool is_null,
    char **out_text,
    bool *out_is_null
) {
    if (out_text == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_is_null = false;
    if (is_null) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    return from_days_result(database, day_number, out_text, out_is_null);
}

int mylite_makedate_value(
    struct mylite_db *database,
    int64_t year,
    bool year_is_null,
    int64_t day_of_year_value,
    bool day_of_year_is_null,
    char **out_text,
    bool *out_is_null
) {
    const struct temporal_constructor_makedate_arguments arguments = {
        .year = year,
        .day_of_year = day_of_year_value,
    };

    if (out_text == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_is_null = false;
    if (year_is_null || day_of_year_is_null) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    return makedate_result(database, &arguments, out_text, out_is_null);
}

int mylite_maketime_value(
    struct mylite_db *database,
    const struct mylite_maketime_arguments *arguments,
    char **out_text,
    bool *out_is_null
) {
    if (arguments == NULL || out_text == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_is_null = false;
    if (arguments->hour_is_null || arguments->minute_is_null || arguments->second_is_null) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    return maketime_result(
        database,
        arguments->hour,
        arguments->minute,
        arguments->second,
        out_text,
        out_is_null
    );
}

int mylite_sqlite_register_temporal_constructor_functions(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_from_days",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = from_days_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_makedate",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = makedate_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_maketime",
            .argument_count = 3,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = maketime_sqlite_callback,
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

static void from_days_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    char *result = NULL;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (database == NULL || argc != 1 || argv == NULL) {
        sqlite3_result_error(context, "MyLite FROM_DAYS() callback failed", -1);
        return;
    }
    rc = mylite_from_days_value(
        database,
        sqlite3_value_int64(argv[0]),
        sqlite3_value_type(argv[0]) == SQLITE_NULL,
        &result,
        &is_null
    );
    (void)temporal_constructor_sqlite_result(context, result, is_null, rc);
}

static void makedate_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    char *result = NULL;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (database == NULL || argc != 2 || argv == NULL) {
        sqlite3_result_error(context, "MyLite MAKEDATE() callback failed", -1);
        return;
    }
    rc = mylite_makedate_value(
        database,
        sqlite3_value_int64(argv[0]),
        sqlite3_value_type(argv[0]) == SQLITE_NULL,
        sqlite3_value_int64(argv[1]),
        sqlite3_value_type(argv[1]) == SQLITE_NULL,
        &result,
        &is_null
    );
    (void)temporal_constructor_sqlite_result(context, result, is_null, rc);
}

static void maketime_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    struct mylite_maketime_arguments arguments = {0};
    char *result = NULL;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (database == NULL || argc != 3 || argv == NULL) {
        sqlite3_result_error(context, "MyLite MAKETIME() callback failed", -1);
        return;
    }
    arguments.hour = sqlite3_value_int64(argv[0]);
    arguments.hour_is_null = sqlite3_value_type(argv[0]) == SQLITE_NULL;
    arguments.minute = sqlite3_value_int64(argv[1]);
    arguments.minute_is_null = sqlite3_value_type(argv[1]) == SQLITE_NULL;
    arguments.second = sqlite3_value_int64(argv[2]);
    arguments.second_is_null = sqlite3_value_type(argv[2]) == SQLITE_NULL;

    rc = mylite_maketime_value(database, &arguments, &result, &is_null);
    (void)temporal_constructor_sqlite_result(context, result, is_null, rc);
}

static int temporal_constructor_sqlite_result(
    sqlite3_context *context,
    char *result,
    bool is_null,
    int rc
) {
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
        } else {
            sqlite3_result_error(context, "MyLite temporal constructor failed", -1);
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

static int from_days_result(
    struct mylite_db *database,
    int64_t day_number,
    char **out_text,
    bool *out_is_null
) {
    struct temporal_constructor_date_parts date = {0};
    int rc = MYLITE_OK;

    if (day_number < from_days_first_valid_day_number ||
        day_number >= from_days_first_zero_after_overflow_day_number) {
        return copy_static_result(database, "0000-00-00", out_text);
    }
    if (day_number > from_days_last_valid_day_number) {
        rc = append_from_days_overflow_warning(database);
        if (rc == MYLITE_OK && out_is_null != NULL) {
            *out_is_null = true;
        }
        return rc;
    }

    date_from_day_number(day_number, &date);
    return format_date_result(database, &date, out_text);
}

static int makedate_result(
    struct mylite_db *database,
    const struct temporal_constructor_makedate_arguments *arguments,
    char **out_text,
    bool *out_is_null
) {
    int converted_year = 0;
    struct temporal_constructor_date_parts jan1 = {
        .year = 0,
        .month = 1,
        .day = 1,
    };
    int64_t jan1_day_number = 0;
    int64_t target_day_number = 0;

    if (arguments == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    converted_year = convert_makedate_year(arguments->year);
    jan1.year = converted_year;
    if (converted_year == 0 || arguments->day_of_year <= 0) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    jan1_day_number = day_number_for_date(&jan1);
    if (arguments->day_of_year > from_days_last_valid_day_number - jan1_day_number + 1) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    target_day_number = jan1_day_number + arguments->day_of_year - 1;
    date_from_day_number(target_day_number, &jan1);
    return format_date_result(database, &jan1, out_text);
}

static int maketime_result(
    struct mylite_db *database,
    int64_t hour,
    int64_t minute,
    int64_t second,
    char **out_text,
    bool *out_is_null
) {
    struct temporal_constructor_time_parts time = {
        .negative = false,
        .hour = 0,
        .minute = 0,
        .second = 0,
    };
    bool clipped = false;
    int rc = MYLITE_OK;

    if (out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    if (minute < 0 || minute > time_minute_second_max || second < 0 ||
        second > time_minute_second_max) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    if (hour < 0) {
        time.negative = true;
        if (hour == INT64_MIN || -hour > time_hour_max) {
            clipped = true;
        } else {
            time.hour = (int)-hour;
        }
    } else if (hour > time_hour_max) {
        clipped = true;
    } else {
        time.hour = (int)hour;
    }

    if (clipped) {
        rc = append_maketime_truncation_warning(database, hour, minute, second);
        if (rc != MYLITE_OK) {
            return rc;
        }
        time.hour = time_hour_max;
        time.minute = time_minute_second_max;
        time.second = time_minute_second_max;
    } else {
        time.minute = (int)minute;
        time.second = (int)second;
    }
    return format_time_result(database, &time, out_text);
}

static int copy_static_result(struct mylite_db *database, const char *value, char **out_text) {
    size_t length = value == NULL ? 0U : strlen(value);

    if (out_text == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = (char *)malloc(length + 1U);
    if (*out_text == NULL) {
        return set_temporal_constructor_nomem_error(database);
    }
    memcpy(*out_text, value, length + 1U);
    return MYLITE_OK;
}

static int format_date_result(
    struct mylite_db *database,
    const struct temporal_constructor_date_parts *parts,
    char **out_text
) {
    char buffer[date_text_length + 1U];
    int written = 0;

    if (out_text == NULL || parts == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    written =
        snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", parts->year, parts->month, parts->day);
    if (written != date_text_length) {
        return MYLITE_ERROR;
    }
    return copy_static_result(database, buffer, out_text);
}

static int format_time_result(
    struct mylite_db *database,
    const struct temporal_constructor_time_parts *parts,
    char **out_text
) {
    char buffer[time_text_max_length + 1U];
    int written = 0;

    if (out_text == NULL || parts == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    written = snprintf(
        buffer,
        sizeof(buffer),
        "%s%02d:%02d:%02d",
        parts->negative ? "-" : "",
        parts->hour,
        parts->minute,
        parts->second
    );
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return MYLITE_ERROR;
    }
    return copy_static_result(database, buffer, out_text);
}

static int append_from_days_overflow_warning(struct mylite_db *database) {
    int rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_datetime_field_overflow,
        "HY000",
        "Datetime function: from_days field overflow"
    );

    if (rc == MYLITE_NOMEM) {
        return set_temporal_constructor_nomem_error(database);
    }
    return rc;
}

static int append_maketime_truncation_warning(
    struct mylite_db *database,
    int64_t hour,
    int64_t minute,
    int64_t second
) {
    char value_text[integer_result_buffer_capacity * 3U];
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        value_text,
        sizeof(value_text),
        "%" PRId64 ":%02" PRId64 ":%02" PRId64,
        hour,
        minute,
        second
    );
    int rc = MYLITE_OK;

    if (written < 0 || (size_t)written >= sizeof(value_text)) {
        return MYLITE_ERROR;
    }
    written =
        snprintf(message, sizeof(message), "Truncated incorrect time value: '%s'", value_text);
    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_incorrect_time_value,
        "22007",
        message
    );
    if (rc == MYLITE_NOMEM) {
        return set_temporal_constructor_nomem_error(database);
    }
    return rc;
}

static int64_t day_number_for_date(const struct temporal_constructor_date_parts *parts) {
    int64_t year = parts == NULL ? 0 : parts->year;
    int64_t years_before = year <= 0 ? 0 : year - 1;
    int64_t leap_days_before_year = (years_before / leap_quadrennial_year_cycle) -
                                    (years_before / leap_century_year_cycle) +
                                    (years_before / leap_quadricentennial_year_cycle);

    if (year > 0) {
        return (year * days_per_common_year) + leap_days_before_year + day_of_year(parts);
    }
    return day_of_year(parts);
}

static void date_from_day_number(
    int64_t day_number,
    struct temporal_constructor_date_parts *out_parts
) {
    int low_year = 1;
    int high_year = date_year_max;
    int year = 1;
    int64_t year_start = 0;
    int day_in_year = 0;

    if (out_parts == NULL) {
        return;
    }
    while (low_year <= high_year) {
        int mid_year = low_year + ((high_year - low_year) / 2);
        struct temporal_constructor_date_parts mid_jan1 = {
            .year = mid_year,
            .month = 1,
            .day = 1,
        };
        int64_t mid_day_number = day_number_for_date(&mid_jan1);

        if (mid_day_number <= day_number) {
            year = mid_year;
            low_year = mid_year + 1;
        } else {
            high_year = mid_year - 1;
        }
    }

    out_parts->year = year;
    out_parts->month = 1;
    out_parts->day = 1;
    year_start = day_number_for_date(out_parts);
    day_in_year = (int)(day_number - year_start) + 1;
    for (int month = 1; month <= date_month_max; ++month) {
        int month_days = days_in_month(year, month);

        if (day_in_year <= month_days) {
            out_parts->month = month;
            out_parts->day = day_in_year;
            return;
        }
        day_in_year -= month_days;
    }
}

static int convert_makedate_year(int64_t year) {
    if (year < 0 || year > date_year_max) {
        return 0;
    }
    if (year < two_digit_year_cutoff) {
        return (int)year + two_digit_year_low_base;
    }
    if (year < two_digit_year_upper_bound) {
        return (int)year + two_digit_year_high_base;
    }
    return (int)year;
}

static bool is_leap_year(int year) {
    if ((year % leap_quadrennial_year_cycle == 0 && year % leap_century_year_cycle != 0) ||
        year % leap_quadricentennial_year_cycle == 0) {
        return true;
    }
    return false;
}

static int days_in_month(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == date_february && is_leap_year(year)) {
        return date_leap_day;
    }
    if (month < 1 || month > date_month_max) {
        return 0;
    }
    return days[month - 1];
}

static int day_of_year(const struct temporal_constructor_date_parts *parts) {
    int day = parts == NULL ? 0 : parts->day;

    if (parts == NULL) {
        return 0;
    }
    for (int month = 1; month < parts->month; ++month) {
        day += days_in_month(parts->year, month);
    }
    return day;
}

static int set_temporal_constructor_nomem_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        MYLITE_NOMEM,
        "HY001",
        "out of memory"
    );
    return MYLITE_NOMEM;
}
