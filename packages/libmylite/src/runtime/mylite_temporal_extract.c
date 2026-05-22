#include "mylite_temporal_extract.h"

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
    mysql_warning_incorrect_temporal_value = 1292,
    date_year_minimum = 1000,
    date_year_maximum = 9999,
    date_month_max = 12,
    date_day_max = 31,
    date_february = 2,
    date_leap_day = 29,
    time_hour_max = 838,
    datetime_hour_max = 23,
    time_minute_second_max = 59,
    date_text_length = 10,
    time_text_min_length = 8,
    time_text_max_length = 9,
    datetime_text_length = 19,
    integer_result_buffer_capacity = 32,
    date_year_digit_count = 4,
    date_first_separator_index = 4,
    date_month_offset = 5,
    date_second_separator_index = 7,
    date_day_offset = 8,
    date_month_day_digit_count = 2,
    time_suffix_length = 6,
    time_minimum_hour_digit_count = 2,
    time_maximum_hour_digit_count = 3,
    time_minute_offset_after_hour = 1,
    time_second_separator_offset_after_hour = 3,
    time_second_offset_after_hour = 4,
    time_minute_second_digit_count = 2,
    time_to_sec_seconds_per_minute = 60,
    time_to_sec_seconds_per_hour = 3600,
    sec_to_time_second_abs_max = 3020399,
    leap_quadrennial_year_cycle = 4,
    leap_century_year_cycle = 100,
    leap_quadricentennial_year_cycle = 400,
    days_per_common_year = 365,
    days_per_week = 7,
    digit_radix = 10,
};

struct temporal_date_parts {
    int year;
    int month;
    int day;
};

struct temporal_time_parts {
    bool negative;
    int hour;
    int minute;
    int second;
};

struct temporal_datetime_parts {
    struct temporal_date_parts date;
    struct temporal_time_parts time;
};

struct temporal_extract_request {
    const char *value;
    size_t value_length;
    enum mylite_temporal_extract_kind extract_kind;
    enum mylite_temporal_extract_input_kind input_kind;
    char **out_text;
    bool *out_is_null;
};

static void temporal_extract_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static void sec_to_time_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static int temporal_extract_sqlite_kind(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_temporal_extract_kind *out_kind
);
static int temporal_extract_sqlite_input_kind(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_temporal_extract_input_kind *out_kind
);
static int temporal_extract_sqlite_result(
    sqlite3_context *context,
    const char *value,
    size_t value_length,
    enum mylite_temporal_extract_kind extract_kind,
    enum mylite_temporal_extract_input_kind input_kind
);
static int sec_to_time_sqlite_result(sqlite3_context *context, int64_t seconds);

static int extract_date_part_value(
    struct mylite_db *database,
    const struct temporal_extract_request *request
);
static int extract_calendar_date_value(
    struct mylite_db *database,
    const struct temporal_extract_request *request
);
static int parse_calendar_date_value(
    const struct temporal_extract_request *request,
    struct temporal_date_parts *out_date
);
static int invalid_calendar_date_value(
    struct mylite_db *database,
    const struct temporal_extract_request *request
);
static int extract_time_part_value(
    struct mylite_db *database,
    const struct temporal_extract_request *request
);
static int extract_time_to_sec_value(
    struct mylite_db *database,
    const struct temporal_extract_request *request
);
static int extract_time_value(
    struct mylite_db *database,
    const struct temporal_extract_request *request
);
static int format_date_result(
    struct mylite_db *database,
    const struct temporal_date_parts *parts,
    char **out_text
);
static int format_time_result(
    struct mylite_db *database,
    const struct temporal_time_parts *parts,
    char **out_text
);
static int format_integer_result(struct mylite_db *database, int value, char **out_text);
static int append_sec_to_time_truncation_warning(struct mylite_db *database, int64_t seconds);

static bool parse_string_date_value(
    const char *value,
    size_t value_length,
    struct temporal_date_parts *out_date
);
static bool parse_calendar_string_date_value(
    const char *value,
    size_t value_length,
    struct temporal_date_parts *out_date
);
static bool parse_string_time_value(
    const char *value,
    size_t value_length,
    struct temporal_time_parts *out_time
);
static bool parse_calendar_datetime_text(
    const char *value,
    size_t value_length,
    struct temporal_datetime_parts *out_parts
);
static bool parse_calendar_date_text(
    const char *value,
    size_t value_length,
    struct temporal_date_parts *out_parts
);
static bool parse_datetime_text(
    const char *value,
    size_t value_length,
    struct temporal_datetime_parts *out_parts
);
static bool parse_date_text(
    const char *value,
    size_t value_length,
    struct temporal_date_parts *out_parts
);
static bool parse_time_text(
    const char *value,
    size_t value_length,
    struct temporal_time_parts *out_parts
);
static bool parse_fixed_digits(const char *text, size_t count, int *out_value);
static bool calendar_complete_date_is_valid(const struct temporal_date_parts *parts);
static bool calendar_last_day_argument_is_valid(const struct temporal_date_parts *parts);
static bool date_parts_are_valid(const struct temporal_date_parts *parts);
static bool datetime_time_parts_are_valid(const struct temporal_time_parts *parts);
static bool time_parts_are_valid(const struct temporal_time_parts *parts);
static bool calendar_is_leap_year(int year);
static bool is_leap_year(int year);
static int calendar_days_in_month(int year, int month);
static int days_in_month(int year, int month);
static int calendar_day_of_year(const struct temporal_date_parts *parts);
static int calendar_day_of_week(const struct temporal_date_parts *parts);
static int64_t calendar_day_number(const struct temporal_date_parts *parts);
static int append_incorrect_temporal_warning(
    struct mylite_db *database,
    const char *prefix,
    const char *value,
    size_t value_length
);

const char *mylite_temporal_extract_kind_name(enum mylite_temporal_extract_kind kind) {
    switch (kind) {
    case MYLITE_TEMPORAL_EXTRACT_DATE:
        return "date";
    case MYLITE_TEMPORAL_EXTRACT_TIME:
        return "time";
    case MYLITE_TEMPORAL_EXTRACT_TIME_TO_SEC:
        return "time_to_sec";
    case MYLITE_TEMPORAL_EXTRACT_YEAR:
        return "year";
    case MYLITE_TEMPORAL_EXTRACT_MONTH:
        return "month";
    case MYLITE_TEMPORAL_EXTRACT_DAY:
        return "day";
    case MYLITE_TEMPORAL_EXTRACT_DAYOFWEEK:
        return "dayofweek";
    case MYLITE_TEMPORAL_EXTRACT_DAYOFYEAR:
        return "dayofyear";
    case MYLITE_TEMPORAL_EXTRACT_LAST_DAY:
        return "last_day";
    case MYLITE_TEMPORAL_EXTRACT_HOUR:
        return "hour";
    case MYLITE_TEMPORAL_EXTRACT_MINUTE:
        return "minute";
    case MYLITE_TEMPORAL_EXTRACT_SECOND:
        return "second";
    }
    return NULL;
}

bool mylite_temporal_extract_kind_from_name(
    const char *name,
    size_t name_length,
    enum mylite_temporal_extract_kind *out_kind
) {
    static const struct {
        const char *name;
        enum mylite_temporal_extract_kind kind;
    } names[] = {
        {"date", MYLITE_TEMPORAL_EXTRACT_DATE},
        {"time", MYLITE_TEMPORAL_EXTRACT_TIME},
        {"year", MYLITE_TEMPORAL_EXTRACT_YEAR},
        {"month", MYLITE_TEMPORAL_EXTRACT_MONTH},
        {"day", MYLITE_TEMPORAL_EXTRACT_DAY},
        {"dayofweek", MYLITE_TEMPORAL_EXTRACT_DAYOFWEEK},
        {"dayofyear", MYLITE_TEMPORAL_EXTRACT_DAYOFYEAR},
        {"last_day", MYLITE_TEMPORAL_EXTRACT_LAST_DAY},
        {"hour", MYLITE_TEMPORAL_EXTRACT_HOUR},
        {"minute", MYLITE_TEMPORAL_EXTRACT_MINUTE},
        {"second", MYLITE_TEMPORAL_EXTRACT_SECOND},
        {"time_to_sec", MYLITE_TEMPORAL_EXTRACT_TIME_TO_SEC},
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

bool mylite_temporal_extract_kind_is_calendar_date(enum mylite_temporal_extract_kind kind) {
    switch (kind) {
    case MYLITE_TEMPORAL_EXTRACT_DAYOFWEEK:
    case MYLITE_TEMPORAL_EXTRACT_DAYOFYEAR:
    case MYLITE_TEMPORAL_EXTRACT_LAST_DAY:
        return true;
    case MYLITE_TEMPORAL_EXTRACT_DATE:
    case MYLITE_TEMPORAL_EXTRACT_TIME:
    case MYLITE_TEMPORAL_EXTRACT_YEAR:
    case MYLITE_TEMPORAL_EXTRACT_MONTH:
    case MYLITE_TEMPORAL_EXTRACT_DAY:
    case MYLITE_TEMPORAL_EXTRACT_HOUR:
    case MYLITE_TEMPORAL_EXTRACT_MINUTE:
    case MYLITE_TEMPORAL_EXTRACT_SECOND:
    case MYLITE_TEMPORAL_EXTRACT_TIME_TO_SEC:
        return false;
    }
    return false;
}

bool mylite_temporal_extract_kind_is_date_part(enum mylite_temporal_extract_kind kind) {
    switch (kind) {
    case MYLITE_TEMPORAL_EXTRACT_DATE:
    case MYLITE_TEMPORAL_EXTRACT_YEAR:
    case MYLITE_TEMPORAL_EXTRACT_MONTH:
    case MYLITE_TEMPORAL_EXTRACT_DAY:
    case MYLITE_TEMPORAL_EXTRACT_DAYOFWEEK:
    case MYLITE_TEMPORAL_EXTRACT_DAYOFYEAR:
    case MYLITE_TEMPORAL_EXTRACT_LAST_DAY:
        return true;
    case MYLITE_TEMPORAL_EXTRACT_TIME:
    case MYLITE_TEMPORAL_EXTRACT_HOUR:
    case MYLITE_TEMPORAL_EXTRACT_MINUTE:
    case MYLITE_TEMPORAL_EXTRACT_SECOND:
    case MYLITE_TEMPORAL_EXTRACT_TIME_TO_SEC:
        return false;
    }
    return false;
}

bool mylite_temporal_extract_kind_is_time_part(enum mylite_temporal_extract_kind kind) {
    switch (kind) {
    case MYLITE_TEMPORAL_EXTRACT_HOUR:
    case MYLITE_TEMPORAL_EXTRACT_MINUTE:
    case MYLITE_TEMPORAL_EXTRACT_SECOND:
        return true;
    case MYLITE_TEMPORAL_EXTRACT_DATE:
    case MYLITE_TEMPORAL_EXTRACT_TIME:
    case MYLITE_TEMPORAL_EXTRACT_YEAR:
    case MYLITE_TEMPORAL_EXTRACT_MONTH:
    case MYLITE_TEMPORAL_EXTRACT_DAY:
    case MYLITE_TEMPORAL_EXTRACT_DAYOFWEEK:
    case MYLITE_TEMPORAL_EXTRACT_DAYOFYEAR:
    case MYLITE_TEMPORAL_EXTRACT_LAST_DAY:
    case MYLITE_TEMPORAL_EXTRACT_TIME_TO_SEC:
        return false;
    }
    return false;
}

const char *mylite_temporal_extract_input_kind_name(enum mylite_temporal_extract_input_kind kind) {
    switch (kind) {
    case MYLITE_TEMPORAL_EXTRACT_INPUT_STRING:
        return "string";
    case MYLITE_TEMPORAL_EXTRACT_INPUT_DATE:
        return "date";
    case MYLITE_TEMPORAL_EXTRACT_INPUT_TIME:
        return "time";
    case MYLITE_TEMPORAL_EXTRACT_INPUT_DATETIME:
        return "datetime";
    case MYLITE_TEMPORAL_EXTRACT_INPUT_TIMESTAMP:
        return "timestamp";
    }
    return NULL;
}

bool mylite_temporal_extract_input_kind_from_name(
    const char *name,
    size_t name_length,
    enum mylite_temporal_extract_input_kind *out_kind
) {
    static const struct {
        const char *name;
        enum mylite_temporal_extract_input_kind kind;
    } names[] = {
        {"string", MYLITE_TEMPORAL_EXTRACT_INPUT_STRING},
        {"date", MYLITE_TEMPORAL_EXTRACT_INPUT_DATE},
        {"time", MYLITE_TEMPORAL_EXTRACT_INPUT_TIME},
        {"datetime", MYLITE_TEMPORAL_EXTRACT_INPUT_DATETIME},
        {"timestamp", MYLITE_TEMPORAL_EXTRACT_INPUT_TIMESTAMP},
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

int mylite_temporal_extract_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    enum mylite_temporal_extract_kind extract_kind,
    enum mylite_temporal_extract_input_kind input_kind,
    char **out_text,
    bool *out_is_null
) {
    struct temporal_extract_request request = {
        .value = value,
        .value_length = value_length,
        .extract_kind = extract_kind,
        .input_kind = input_kind,
        .out_text = out_text,
        .out_is_null = out_is_null,
    };

    if (out_text == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_is_null = false;
    if (value == NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (mylite_temporal_extract_kind_is_date_part(extract_kind)) {
        return extract_date_part_value(database, &request);
    }
    return extract_time_part_value(database, &request);
}

int mylite_sec_to_time_value(
    struct mylite_db *database,
    int64_t seconds,
    bool is_null,
    char **out_text,
    bool *out_is_null
) {
    struct temporal_time_parts time = {.negative = false};
    int64_t magnitude = seconds;
    bool clipped = false;
    int rc = MYLITE_OK;

    if (out_text == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_is_null = false;
    if (is_null) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    if (seconds < 0) {
        time.negative = true;
        if (seconds < -((int64_t)sec_to_time_second_abs_max)) {
            magnitude = sec_to_time_second_abs_max;
            clipped = true;
        } else {
            magnitude = -seconds;
        }
    } else if (seconds > sec_to_time_second_abs_max) {
        magnitude = sec_to_time_second_abs_max;
        clipped = true;
    }

    if (clipped) {
        rc = append_sec_to_time_truncation_warning(database, seconds);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    time.hour = (int)(magnitude / time_to_sec_seconds_per_hour);
    magnitude %= time_to_sec_seconds_per_hour;
    time.minute = (int)(magnitude / time_to_sec_seconds_per_minute);
    time.second = (int)(magnitude % time_to_sec_seconds_per_minute);
    return format_time_result(database, &time, out_text);
}

int mylite_sqlite_register_temporal_extract_function(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_temporal_extract",
            .argument_count = 3,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = temporal_extract_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_sec_to_time",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = sec_to_time_sqlite_callback,
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

static void temporal_extract_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    enum mylite_temporal_extract_kind extract_kind = MYLITE_TEMPORAL_EXTRACT_DATE;
    enum mylite_temporal_extract_input_kind input_kind = MYLITE_TEMPORAL_EXTRACT_INPUT_STRING;
    const unsigned char *value = NULL;
    int value_length = 0;

    if (context == NULL || argc != 3 || argv == NULL || argv[0] == NULL || argv[1] == NULL ||
        argv[2] == NULL) {
        sqlite3_result_error(context, "invalid MyLite temporal extract callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL ||
        sqlite3_value_type(argv[2]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    if (temporal_extract_sqlite_kind(context, argv[1], &extract_kind) != MYLITE_OK ||
        temporal_extract_sqlite_input_kind(context, argv[2], &input_kind) != MYLITE_OK) {
        return;
    }

    value = sqlite3_value_text(argv[0]);
    value_length = sqlite3_value_bytes(argv[0]);
    if (value == NULL || value_length < 0) {
        sqlite3_result_error_nomem(context);
        return;
    }

    if (temporal_extract_sqlite_result(
            context,
            (const char *)value,
            (size_t)value_length,
            extract_kind,
            input_kind
        ) != MYLITE_OK) {
        return;
    }
}

static void sec_to_time_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite SEC_TO_TIME callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    if (sqlite3_value_type(argv[0]) != SQLITE_INTEGER) {
        sqlite3_result_error(context, "invalid MyLite SEC_TO_TIME input", -1);
        return;
    }

    (void)sec_to_time_sqlite_result(context, sqlite3_value_int64(argv[0]));
}

static int temporal_extract_sqlite_kind(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_temporal_extract_kind *out_kind
) {
    const unsigned char *kind_text = sqlite3_value_text(value);
    int kind_length = sqlite3_value_bytes(value);

    if (kind_text == NULL || kind_length < 0 || out_kind == NULL) {
        sqlite3_result_error_nomem(context);
        return MYLITE_NOMEM;
    }
    if (!mylite_temporal_extract_kind_from_name(
            (const char *)kind_text,
            (size_t)kind_length,
            out_kind
        )) {
        sqlite3_result_error(context, "invalid MyLite temporal extract kind", -1);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int temporal_extract_sqlite_input_kind(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_temporal_extract_input_kind *out_kind
) {
    const unsigned char *kind_text = sqlite3_value_text(value);
    int kind_length = sqlite3_value_bytes(value);

    if (kind_text == NULL || kind_length < 0 || out_kind == NULL) {
        sqlite3_result_error_nomem(context);
        return MYLITE_NOMEM;
    }
    if (!mylite_temporal_extract_input_kind_from_name(
            (const char *)kind_text,
            (size_t)kind_length,
            out_kind
        )) {
        sqlite3_result_error(context, "invalid MyLite temporal extract input kind", -1);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int sec_to_time_sqlite_result(sqlite3_context *context, int64_t seconds) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    char *result = NULL;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite SEC_TO_TIME owner", -1);
        return MYLITE_ERROR;
    }

    rc = mylite_sec_to_time_value(database, seconds, false, &result, &is_null);
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
        } else {
            sqlite3_result_error(context, "MyLite SEC_TO_TIME failed", -1);
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

static int temporal_extract_sqlite_result(
    sqlite3_context *context,
    const char *value,
    size_t value_length,
    enum mylite_temporal_extract_kind extract_kind,
    enum mylite_temporal_extract_input_kind input_kind
) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    char *result = NULL;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite temporal extract owner", -1);
        return MYLITE_ERROR;
    }

    rc = mylite_temporal_extract_value(
        database,
        value,
        value_length,
        extract_kind,
        input_kind,
        &result,
        &is_null
    );
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
        } else {
            sqlite3_result_error(context, "MyLite temporal extract failed", -1);
        }
        free(result);
        return rc;
    }
    if (is_null) {
        sqlite3_result_null(context);
    } else if (
        extract_kind == MYLITE_TEMPORAL_EXTRACT_DATE ||
        extract_kind == MYLITE_TEMPORAL_EXTRACT_TIME ||
        extract_kind == MYLITE_TEMPORAL_EXTRACT_LAST_DAY
    ) {
        sqlite3_result_text(context, result, -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_result_int64(context, (sqlite3_int64)strtoll(result, NULL, digit_radix));
    }
    free(result);
    return MYLITE_OK;
}

static int extract_date_part_value(
    struct mylite_db *database,
    const struct temporal_extract_request *request
) {
    struct temporal_date_parts date = {0};
    struct temporal_datetime_parts datetime = {0};
    int rc = MYLITE_OK;

    if (mylite_temporal_extract_kind_is_calendar_date(request->extract_kind)) {
        return extract_calendar_date_value(database, request);
    }
    if (request->input_kind == MYLITE_TEMPORAL_EXTRACT_INPUT_DATETIME ||
        request->input_kind == MYLITE_TEMPORAL_EXTRACT_INPUT_TIMESTAMP) {
        if (parse_datetime_text(request->value, request->value_length, &datetime)) {
            date = datetime.date;
        } else {
            rc = append_incorrect_temporal_warning(
                database,
                "Incorrect datetime value",
                request->value,
                request->value_length
            );
            *request->out_is_null = true;
            return rc;
        }
    } else if (request->input_kind == MYLITE_TEMPORAL_EXTRACT_INPUT_DATE) {
        if (!parse_date_text(request->value, request->value_length, &date)) {
            rc = append_incorrect_temporal_warning(
                database,
                "Incorrect datetime value",
                request->value,
                request->value_length
            );
            *request->out_is_null = true;
            return rc;
        }
    } else if (!parse_string_date_value(request->value, request->value_length, &date)) {
        rc = append_incorrect_temporal_warning(
            database,
            "Incorrect datetime value",
            request->value,
            request->value_length
        );
        *request->out_is_null = true;
        return rc;
    }

    switch (request->extract_kind) {
    case MYLITE_TEMPORAL_EXTRACT_DATE:
        return format_date_result(database, &date, request->out_text);
    case MYLITE_TEMPORAL_EXTRACT_YEAR:
        return format_integer_result(database, date.year, request->out_text);
    case MYLITE_TEMPORAL_EXTRACT_MONTH:
        return format_integer_result(database, date.month, request->out_text);
    case MYLITE_TEMPORAL_EXTRACT_DAY:
        return format_integer_result(database, date.day, request->out_text);
    case MYLITE_TEMPORAL_EXTRACT_DAYOFWEEK:
    case MYLITE_TEMPORAL_EXTRACT_DAYOFYEAR:
    case MYLITE_TEMPORAL_EXTRACT_LAST_DAY:
    case MYLITE_TEMPORAL_EXTRACT_TIME:
    case MYLITE_TEMPORAL_EXTRACT_HOUR:
    case MYLITE_TEMPORAL_EXTRACT_MINUTE:
    case MYLITE_TEMPORAL_EXTRACT_SECOND:
    case MYLITE_TEMPORAL_EXTRACT_TIME_TO_SEC:
        break;
    }
    return MYLITE_ERROR;
}

static int extract_calendar_date_value(
    struct mylite_db *database,
    const struct temporal_extract_request *request
) {
    struct temporal_date_parts date = {0};

    if (parse_calendar_date_value(request, &date) != MYLITE_OK) {
        return invalid_calendar_date_value(database, request);
    }

    switch (request->extract_kind) {
    case MYLITE_TEMPORAL_EXTRACT_DAYOFWEEK:
        if (!calendar_complete_date_is_valid(&date)) {
            return invalid_calendar_date_value(database, request);
        }
        return format_integer_result(database, calendar_day_of_week(&date), request->out_text);
    case MYLITE_TEMPORAL_EXTRACT_DAYOFYEAR:
        if (!calendar_complete_date_is_valid(&date)) {
            return invalid_calendar_date_value(database, request);
        }
        return format_integer_result(database, calendar_day_of_year(&date), request->out_text);
    case MYLITE_TEMPORAL_EXTRACT_LAST_DAY:
        if (!calendar_last_day_argument_is_valid(&date)) {
            return invalid_calendar_date_value(database, request);
        }
        date.day = calendar_days_in_month(date.year, date.month);
        return format_date_result(database, &date, request->out_text);
    case MYLITE_TEMPORAL_EXTRACT_DATE:
    case MYLITE_TEMPORAL_EXTRACT_TIME:
    case MYLITE_TEMPORAL_EXTRACT_YEAR:
    case MYLITE_TEMPORAL_EXTRACT_MONTH:
    case MYLITE_TEMPORAL_EXTRACT_DAY:
    case MYLITE_TEMPORAL_EXTRACT_HOUR:
    case MYLITE_TEMPORAL_EXTRACT_MINUTE:
    case MYLITE_TEMPORAL_EXTRACT_SECOND:
    case MYLITE_TEMPORAL_EXTRACT_TIME_TO_SEC:
        break;
    }
    return MYLITE_ERROR;
}

static int parse_calendar_date_value(
    const struct temporal_extract_request *request,
    struct temporal_date_parts *out_date
) {
    struct temporal_datetime_parts datetime = {0};

    if (request == NULL || out_date == NULL) {
        return MYLITE_MISUSE;
    }
    if (request->input_kind == MYLITE_TEMPORAL_EXTRACT_INPUT_DATETIME ||
        request->input_kind == MYLITE_TEMPORAL_EXTRACT_INPUT_TIMESTAMP) {
        if (parse_calendar_datetime_text(request->value, request->value_length, &datetime)) {
            *out_date = datetime.date;
            return MYLITE_OK;
        }
        return MYLITE_ERROR;
    }
    if (request->input_kind == MYLITE_TEMPORAL_EXTRACT_INPUT_DATE) {
        if (parse_calendar_date_text(request->value, request->value_length, out_date)) {
            return MYLITE_OK;
        }
        return MYLITE_ERROR;
    }
    if (parse_calendar_string_date_value(request->value, request->value_length, out_date)) {
        return MYLITE_OK;
    }
    return MYLITE_ERROR;
}

static int invalid_calendar_date_value(
    struct mylite_db *database,
    const struct temporal_extract_request *request
) {
    int rc = MYLITE_OK;

    if (request == NULL) {
        return MYLITE_MISUSE;
    }
    if (request->input_kind == MYLITE_TEMPORAL_EXTRACT_INPUT_STRING) {
        rc = append_incorrect_temporal_warning(
            database,
            "Incorrect datetime value",
            request->value,
            request->value_length
        );
    }
    *request->out_is_null = true;
    return rc;
}

static int extract_time_part_value(
    struct mylite_db *database,
    const struct temporal_extract_request *request
) {
    struct temporal_time_parts time = {.negative = false};
    struct temporal_datetime_parts datetime = {0};
    int rc = MYLITE_OK;

    if (request->extract_kind == MYLITE_TEMPORAL_EXTRACT_TIME) {
        return extract_time_value(database, request);
    }
    if (request->extract_kind == MYLITE_TEMPORAL_EXTRACT_TIME_TO_SEC) {
        return extract_time_to_sec_value(database, request);
    }

    if (request->input_kind == MYLITE_TEMPORAL_EXTRACT_INPUT_DATETIME ||
        request->input_kind == MYLITE_TEMPORAL_EXTRACT_INPUT_TIMESTAMP) {
        if (parse_datetime_text(request->value, request->value_length, &datetime) &&
            datetime_time_parts_are_valid(&datetime.time)) {
            time = datetime.time;
        } else {
            rc = append_incorrect_temporal_warning(
                database,
                "Truncated incorrect time value",
                request->value,
                request->value_length
            );
            *request->out_is_null = true;
            return rc;
        }
    } else if (request->input_kind == MYLITE_TEMPORAL_EXTRACT_INPUT_TIME) {
        if (!parse_time_text(request->value, request->value_length, &time)) {
            rc = append_incorrect_temporal_warning(
                database,
                "Truncated incorrect time value",
                request->value,
                request->value_length
            );
            *request->out_is_null = true;
            return rc;
        }
    } else if (!parse_string_time_value(request->value, request->value_length, &time)) {
        rc = append_incorrect_temporal_warning(
            database,
            "Truncated incorrect time value",
            request->value,
            request->value_length
        );
        *request->out_is_null = true;
        return rc;
    }

    switch (request->extract_kind) {
    case MYLITE_TEMPORAL_EXTRACT_HOUR:
        return format_integer_result(database, time.hour, request->out_text);
    case MYLITE_TEMPORAL_EXTRACT_MINUTE:
        return format_integer_result(database, time.minute, request->out_text);
    case MYLITE_TEMPORAL_EXTRACT_SECOND:
        return format_integer_result(database, time.second, request->out_text);
    case MYLITE_TEMPORAL_EXTRACT_DATE:
    case MYLITE_TEMPORAL_EXTRACT_TIME:
    case MYLITE_TEMPORAL_EXTRACT_YEAR:
    case MYLITE_TEMPORAL_EXTRACT_MONTH:
    case MYLITE_TEMPORAL_EXTRACT_DAY:
    case MYLITE_TEMPORAL_EXTRACT_DAYOFWEEK:
    case MYLITE_TEMPORAL_EXTRACT_DAYOFYEAR:
    case MYLITE_TEMPORAL_EXTRACT_LAST_DAY:
    case MYLITE_TEMPORAL_EXTRACT_TIME_TO_SEC:
        break;
    }
    return MYLITE_ERROR;
}

static int extract_time_to_sec_value(
    struct mylite_db *database,
    const struct temporal_extract_request *request
) {
    struct temporal_time_parts time = {.negative = false};
    struct temporal_datetime_parts datetime = {0};
    int total_seconds = 0;
    int rc = MYLITE_OK;

    if (request->input_kind == MYLITE_TEMPORAL_EXTRACT_INPUT_DATE) {
        struct temporal_date_parts date = {0};

        if (!parse_date_text(request->value, request->value_length, &date)) {
            rc = append_incorrect_temporal_warning(
                database,
                "Truncated incorrect time value",
                request->value,
                request->value_length
            );
            *request->out_is_null = true;
            return rc;
        }
        return format_integer_result(database, 0, request->out_text);
    }
    if (request->input_kind == MYLITE_TEMPORAL_EXTRACT_INPUT_DATETIME ||
        request->input_kind == MYLITE_TEMPORAL_EXTRACT_INPUT_TIMESTAMP) {
        if (parse_datetime_text(request->value, request->value_length, &datetime) &&
            datetime_time_parts_are_valid(&datetime.time)) {
            time = datetime.time;
        } else {
            rc = append_incorrect_temporal_warning(
                database,
                "Truncated incorrect time value",
                request->value,
                request->value_length
            );
            *request->out_is_null = true;
            return rc;
        }
    } else if (request->input_kind == MYLITE_TEMPORAL_EXTRACT_INPUT_TIME) {
        if (!parse_time_text(request->value, request->value_length, &time)) {
            rc = append_incorrect_temporal_warning(
                database,
                "Truncated incorrect time value",
                request->value,
                request->value_length
            );
            *request->out_is_null = true;
            return rc;
        }
    } else if (!parse_string_time_value(request->value, request->value_length, &time)) {
        rc = append_incorrect_temporal_warning(
            database,
            "Truncated incorrect time value",
            request->value,
            request->value_length
        );
        *request->out_is_null = true;
        return rc;
    }

    total_seconds = (time.hour * time_to_sec_seconds_per_hour) +
                    (time.minute * time_to_sec_seconds_per_minute) + time.second;
    if (time.negative) {
        total_seconds = -total_seconds;
    }
    return format_integer_result(database, total_seconds, request->out_text);
}

static int extract_time_value(
    struct mylite_db *database,
    const struct temporal_extract_request *request
) {
    struct temporal_time_parts time = {.negative = false};
    struct temporal_datetime_parts datetime = {0};
    int rc = MYLITE_OK;

    if (request->input_kind == MYLITE_TEMPORAL_EXTRACT_INPUT_DATETIME ||
        request->input_kind == MYLITE_TEMPORAL_EXTRACT_INPUT_TIMESTAMP) {
        if (parse_datetime_text(request->value, request->value_length, &datetime)) {
            time = datetime.time;
        } else {
            rc = append_incorrect_temporal_warning(
                database,
                "Truncated incorrect time value",
                request->value,
                request->value_length
            );
            *request->out_is_null = true;
            return rc;
        }
    } else if (request->input_kind == MYLITE_TEMPORAL_EXTRACT_INPUT_DATE) {
        struct temporal_date_parts date = {0};

        if (!parse_date_text(request->value, request->value_length, &date)) {
            rc = append_incorrect_temporal_warning(
                database,
                "Truncated incorrect time value",
                request->value,
                request->value_length
            );
            *request->out_is_null = true;
            return rc;
        }
    } else if (request->input_kind == MYLITE_TEMPORAL_EXTRACT_INPUT_TIME) {
        if (!parse_time_text(request->value, request->value_length, &time)) {
            rc = append_incorrect_temporal_warning(
                database,
                "Truncated incorrect time value",
                request->value,
                request->value_length
            );
            *request->out_is_null = true;
            return rc;
        }
    } else if (!parse_string_time_value(request->value, request->value_length, &time)) {
        rc = append_incorrect_temporal_warning(
            database,
            "Truncated incorrect time value",
            request->value,
            request->value_length
        );
        *request->out_is_null = true;
        return rc;
    }

    return format_time_result(database, &time, request->out_text);
}

static int format_date_result(
    struct mylite_db *database,
    const struct temporal_date_parts *parts,
    char **out_text
) {
    int written = 0;

    if (out_text == NULL || parts == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = (char *)malloc((size_t)date_text_length + 1U);
    if (*out_text == NULL) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_NOMEM,
            "HY001",
            "out of memory"
        );
        return MYLITE_NOMEM;
    }
    written = snprintf(
        *out_text,
        (size_t)date_text_length + 1U,
        "%04d-%02d-%02d",
        parts->year,
        parts->month,
        parts->day
    );
    if (written != date_text_length) {
        free(*out_text);
        *out_text = NULL;
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int format_time_result(
    struct mylite_db *database,
    const struct temporal_time_parts *parts,
    char **out_text
) {
    char buffer[integer_result_buffer_capacity];
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
    *out_text = (char *)malloc((size_t)written + 1U);
    if (*out_text == NULL) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_NOMEM,
            "HY001",
            "out of memory"
        );
        return MYLITE_NOMEM;
    }
    memcpy(*out_text, buffer, (size_t)written + 1U);
    return MYLITE_OK;
}

static int format_integer_result(struct mylite_db *database, int value, char **out_text) {
    char buffer[integer_result_buffer_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%d", value);

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return MYLITE_ERROR;
    }
    *out_text = (char *)malloc((size_t)written + 1U);
    if (*out_text == NULL) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_NOMEM,
            "HY001",
            "out of memory"
        );
        return MYLITE_NOMEM;
    }
    memcpy(*out_text, buffer, (size_t)written + 1U);
    return MYLITE_OK;
}

static int append_sec_to_time_truncation_warning(struct mylite_db *database, int64_t seconds) {
    char value_text[integer_result_buffer_capacity];
    int written = snprintf(value_text, sizeof(value_text), "%" PRId64, seconds);

    if (written < 0 || (size_t)written >= sizeof(value_text)) {
        return MYLITE_ERROR;
    }
    return append_incorrect_temporal_warning(
        database,
        "Truncated incorrect time value",
        value_text,
        (size_t)written
    );
}

static bool parse_string_date_value(
    const char *value,
    size_t value_length,
    struct temporal_date_parts *out_date
) {
    struct temporal_datetime_parts datetime = {0};

    if (parse_date_text(value, value_length, out_date)) {
        return true;
    }
    if (parse_datetime_text(value, value_length, &datetime)) {
        *out_date = datetime.date;
        return true;
    }
    return false;
}

static bool parse_calendar_string_date_value(
    const char *value,
    size_t value_length,
    struct temporal_date_parts *out_date
) {
    struct temporal_datetime_parts datetime = {0};

    if (parse_calendar_date_text(value, value_length, out_date)) {
        return true;
    }
    if (parse_calendar_datetime_text(value, value_length, &datetime)) {
        *out_date = datetime.date;
        return true;
    }
    return false;
}

static bool parse_string_time_value(
    const char *value,
    size_t value_length,
    struct temporal_time_parts *out_time
) {
    struct temporal_datetime_parts datetime = {0};

    if (parse_time_text(value, value_length, out_time)) {
        return true;
    }
    if (parse_datetime_text(value, value_length, &datetime) &&
        datetime_time_parts_are_valid(&datetime.time)) {
        *out_time = datetime.time;
        return true;
    }
    return false;
}

static bool parse_calendar_datetime_text(
    const char *value,
    size_t value_length,
    struct temporal_datetime_parts *out_parts
) {
    struct temporal_datetime_parts parts = {0};

    if (value == NULL || out_parts == NULL || value_length != datetime_text_length ||
        value[date_text_length] != ' ') {
        return false;
    }
    if (!parse_calendar_date_text(value, date_text_length, &parts.date) ||
        !parse_time_text(value + date_text_length + 1U, time_text_min_length, &parts.time) ||
        !datetime_time_parts_are_valid(&parts.time)) {
        return false;
    }
    *out_parts = parts;
    return true;
}

static bool parse_calendar_date_text(
    const char *value,
    size_t value_length,
    struct temporal_date_parts *out_parts
) {
    struct temporal_date_parts parts = {0};

    if (value == NULL || out_parts == NULL || value_length != date_text_length ||
        value[date_first_separator_index] != '-' || value[date_second_separator_index] != '-') {
        return false;
    }
    if (!parse_fixed_digits(value, date_year_digit_count, &parts.year) ||
        !parse_fixed_digits(value + date_month_offset, date_month_day_digit_count, &parts.month) ||
        !parse_fixed_digits(value + date_day_offset, date_month_day_digit_count, &parts.day) ||
        parts.month < 0 || parts.month > date_month_max || parts.day < 0 ||
        parts.day > date_day_max) {
        return false;
    }
    *out_parts = parts;
    return true;
}

static bool parse_datetime_text(
    const char *value,
    size_t value_length,
    struct temporal_datetime_parts *out_parts
) {
    struct temporal_datetime_parts parts = {0};

    if (value == NULL || out_parts == NULL || value_length != datetime_text_length ||
        value[date_text_length] != ' ') {
        return false;
    }
    if (!parse_date_text(value, date_text_length, &parts.date) ||
        !parse_time_text(value + date_text_length + 1U, time_text_min_length, &parts.time)) {
        return false;
    }
    *out_parts = parts;
    return true;
}

static bool parse_date_text(
    const char *value,
    size_t value_length,
    struct temporal_date_parts *out_parts
) {
    struct temporal_date_parts parts = {0};

    if (value == NULL || out_parts == NULL || value_length != date_text_length ||
        value[date_first_separator_index] != '-' || value[date_second_separator_index] != '-') {
        return false;
    }
    if (!parse_fixed_digits(value, date_year_digit_count, &parts.year) ||
        !parse_fixed_digits(value + date_month_offset, date_month_day_digit_count, &parts.month) ||
        !parse_fixed_digits(value + date_day_offset, date_month_day_digit_count, &parts.day) ||
        !date_parts_are_valid(&parts)) {
        return false;
    }
    *out_parts = parts;
    return true;
}

static bool parse_time_text(
    const char *value,
    size_t value_length,
    struct temporal_time_parts *out_parts
) {
    struct temporal_time_parts parts = {.negative = false};
    size_t offset = 0U;
    size_t hour_digits = 0U;

    if (value == NULL || out_parts == NULL) {
        return false;
    }
    if (value_length > 0U && value[0] == '-') {
        parts.negative = true;
        offset = 1U;
    }
    if (value_length < offset + time_text_min_length ||
        value_length > offset + time_text_max_length) {
        return false;
    }
    hour_digits = value_length - offset - time_suffix_length;
    if ((hour_digits != time_minimum_hour_digit_count &&
         hour_digits != time_maximum_hour_digit_count) ||
        value[offset + hour_digits] != ':' ||
        value[offset + hour_digits + time_second_separator_offset_after_hour] != ':') {
        return false;
    }
    if (!parse_fixed_digits(value + offset, hour_digits, &parts.hour) ||
        !parse_fixed_digits(
            value + offset + hour_digits + time_minute_offset_after_hour,
            time_minute_second_digit_count,
            &parts.minute
        ) ||
        !parse_fixed_digits(
            value + offset + hour_digits + time_second_offset_after_hour,
            time_minute_second_digit_count,
            &parts.second
        ) ||
        !time_parts_are_valid(&parts)) {
        return false;
    }
    *out_parts = parts;
    return true;
}

static bool parse_fixed_digits(const char *text, size_t count, int *out_value) {
    int value = 0;

    if (text == NULL || out_value == NULL || count == 0U) {
        return false;
    }
    for (size_t index = 0U; index < count; ++index) {
        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
        value = (value * digit_radix) + (text[index] - '0');
    }
    *out_value = value;
    return true;
}

static bool calendar_complete_date_is_valid(const struct temporal_date_parts *parts) {
    if (parts == NULL || parts->month < 1 || parts->month > date_month_max || parts->day < 1 ||
        parts->day > date_day_max) {
        return false;
    }
    return parts->day <= calendar_days_in_month(parts->year, parts->month);
}

static bool calendar_last_day_argument_is_valid(const struct temporal_date_parts *parts) {
    if (parts == NULL || parts->month < 1 || parts->month > date_month_max || parts->day < 0 ||
        parts->day > date_day_max) {
        return false;
    }
    if (parts->day == 0) {
        return true;
    }
    return parts->day <= calendar_days_in_month(parts->year, parts->month);
}

static bool date_parts_are_valid(const struct temporal_date_parts *parts) {
    if (parts == NULL || parts->month < 0 || parts->month > date_month_max || parts->day < 0 ||
        parts->day > date_day_max) {
        return false;
    }
    if (parts->year != 0 && (parts->year < date_year_minimum || parts->year > date_year_maximum)) {
        return false;
    }
    if (parts->month == 0 || parts->day == 0) {
        return true;
    }
    return parts->day <= days_in_month(parts->year, parts->month);
}

static bool datetime_time_parts_are_valid(const struct temporal_time_parts *parts) {
    if (parts == NULL || parts->negative || parts->hour < 0 || parts->hour > datetime_hour_max ||
        parts->minute < 0 || parts->minute > time_minute_second_max || parts->second < 0 ||
        parts->second > time_minute_second_max) {
        return false;
    }
    return true;
}

static bool time_parts_are_valid(const struct temporal_time_parts *parts) {
    if (parts == NULL) {
        return false;
    }
    if (parts->hour < 0 || parts->hour > time_hour_max || parts->minute < 0 ||
        parts->minute > time_minute_second_max || parts->second < 0 ||
        parts->second > time_minute_second_max) {
        return false;
    }
    return true;
}

static bool calendar_is_leap_year(int year) {
    if (year == 0) {
        return false;
    }
    return is_leap_year(year);
}

static bool is_leap_year(int year) {
    if ((year % leap_quadrennial_year_cycle == 0 && year % leap_century_year_cycle != 0) ||
        year % leap_quadricentennial_year_cycle == 0) {
        return true;
    }
    return false;
}

static int calendar_days_in_month(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == date_february && calendar_is_leap_year(year)) {
        return date_leap_day;
    }
    if (month < 1 || month > date_month_max) {
        return 0;
    }
    return days[month - 1];
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

static int calendar_day_of_year(const struct temporal_date_parts *parts) {
    int day = parts == NULL ? 0 : parts->day;

    if (parts == NULL) {
        return 0;
    }
    for (int month = 1; month < parts->month; ++month) {
        day += calendar_days_in_month(parts->year, month);
    }
    return day;
}

static int calendar_day_of_week(const struct temporal_date_parts *parts) {
    int value = (int)(calendar_day_number(parts) % days_per_week);

    return value == 0 ? days_per_week : value;
}

static int64_t calendar_day_number(const struct temporal_date_parts *parts) {
    int64_t year = parts == NULL ? 0 : parts->year;
    int64_t years_before = year <= 0 ? 0 : year - 1;
    int64_t leap_days_before_year = (years_before / leap_quadrennial_year_cycle) -
                                    (years_before / leap_century_year_cycle) +
                                    (years_before / leap_quadricentennial_year_cycle);

    if (year > 0) {
        return (year * days_per_common_year) + leap_days_before_year + calendar_day_of_year(parts);
    }
    return calendar_day_of_year(parts);
}

static int append_incorrect_temporal_warning(
    struct mylite_db *database,
    const char *prefix,
    const char *value,
    size_t value_length
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "%s: '%.*s'",
        prefix == NULL ? "Incorrect datetime value" : prefix,
        value_length > 200U ? 200 : (int)value_length,
        value == NULL ? "" : value
    );
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_incorrect_temporal_value,
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
