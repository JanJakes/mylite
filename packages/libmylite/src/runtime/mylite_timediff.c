#include "mylite_timediff.h"

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
    mysql_warning_incorrect_time_value = 1292,
    timediff_date_text_length = 10,
    timediff_datetime_text_length = 19,
    timediff_first_date_separator = 4,
    timediff_month_offset = 5,
    timediff_second_date_separator = 7,
    timediff_day_offset = 8,
    timediff_time_separator = 10,
    timediff_hour_offset = 11,
    timediff_first_time_separator = 13,
    timediff_minute_offset = 14,
    timediff_second_time_separator = 16,
    timediff_second_offset = 17,
    timediff_two_digit_count = 2,
    timediff_four_digit_count = 4,
    timediff_date_year_minimum = 1000,
    timediff_date_year_maximum = 9999,
    timediff_month_maximum = 12,
    timediff_day_maximum = 31,
    timediff_february = 2,
    timediff_leap_day = 29,
    timediff_datetime_hour_maximum = 23,
    timediff_time_hour_maximum = 838,
    timediff_minute_second_maximum = 59,
    timediff_time_text_minimum_length = 8,
    timediff_time_text_maximum_length = 9,
    timediff_time_suffix_length = 6,
    timediff_digit_radix = 10,
    timediff_seconds_per_minute = 60,
    timediff_seconds_per_hour = 3600,
    timediff_seconds_per_day = 86400,
    timediff_maximum_second = 3020399,
    timediff_days_per_common_year = 365,
    timediff_leap_quadrennial_year_cycle = 4,
    timediff_leap_century_year_cycle = 100,
    timediff_leap_quadricentennial_year_cycle = 400,
    timediff_result_capacity = 64,
    timediff_warning_value_preview_length = 200,
    timediff_sqlite_argument_count = 4,
};

enum timediff_domain {
    TIMEDIFF_DOMAIN_NONE = 0,
    TIMEDIFF_DOMAIN_TIME = 1,
    TIMEDIFF_DOMAIN_DATE = 2,
    TIMEDIFF_DOMAIN_DATETIME = 3,
};

struct timediff_date_parts {
    int year;
    int month;
    int day;
};

struct timediff_time_parts {
    int hour;
    int minute;
    int second;
};

struct timediff_datetime_parts {
    struct timediff_date_parts date;
    struct timediff_time_parts time;
};

struct timediff_value_source {
    const char *value;
    size_t value_length;
    enum mylite_timediff_input_kind input_kind;
    bool is_null;
};

struct timediff_value_parts {
    enum timediff_domain domain;
    int64_t time_seconds;
    struct timediff_datetime_parts datetime;
};

enum timediff_parse_status {
    TIMEDIFF_PARSE_VALID = 0,
    TIMEDIFF_PARSE_NULL = 1,
    TIMEDIFF_PARSE_INVALID = 2,
};

static void timediff_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static int timediff_sqlite_input_kind(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_timediff_input_kind *out_kind
);
static int timediff_sqlite_result(
    sqlite3_context *context,
    sqlite3_value *left_value,
    enum mylite_timediff_input_kind left_kind,
    sqlite3_value *right_value,
    enum mylite_timediff_input_kind right_kind
);
static int sqlite_value_text_pointer(
    sqlite3_context *context,
    sqlite3_value *value,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);

static int timediff_value_result(
    struct mylite_db *database,
    const struct timediff_value_source *left,
    const struct timediff_value_source *right,
    char **out_text,
    bool *out_is_null
);
static int parse_timediff_argument(
    struct mylite_db *database,
    const struct timediff_value_source *source,
    struct timediff_value_parts *out_parts
);
static enum timediff_parse_status parse_timediff_value(
    const struct timediff_value_source *source,
    struct timediff_value_parts *out_parts
);
static enum timediff_parse_status parse_string_timediff_value(
    const char *value,
    size_t value_length,
    struct timediff_value_parts *out_parts
);
static enum timediff_parse_status parse_date_timediff_value(
    const char *value,
    size_t value_length,
    struct timediff_value_parts *out_parts
);
static enum timediff_parse_status parse_datetime_timediff_value(
    const char *value,
    size_t value_length,
    struct timediff_value_parts *out_parts
);
static enum timediff_parse_status parse_time_timediff_value(
    const char *value,
    size_t value_length,
    struct timediff_value_parts *out_parts
);
static bool parse_date_text(
    const char *value,
    size_t value_length,
    struct timediff_date_parts *out_date
);
static bool parse_time_text(
    const char *value,
    size_t value_length,
    bool allow_sign,
    struct timediff_time_parts *out_time,
    bool *out_negative
);
static bool parse_digits(const char *text, size_t length, int *out_value);
static bool date_is_valid(const struct timediff_date_parts *date);
static int days_in_month(int year, int month);
static bool is_leap_year(int year);
static int day_of_year(const struct timediff_date_parts *date);
static int64_t day_number(const struct timediff_date_parts *date);
static int64_t seconds_of_day(const struct timediff_time_parts *time);
static int64_t datetime_second_number(const struct timediff_datetime_parts *datetime);
static int timediff_result_seconds(
    const struct timediff_value_parts *left,
    const struct timediff_value_parts *right,
    bool *out_is_null,
    int64_t *out_seconds
);
static int format_timediff_result(struct mylite_db *database, int64_t seconds, char **out_text);
static int format_time_text(struct mylite_db *database, int64_t seconds, char **out_text);
static int append_timediff_truncation_warning(struct mylite_db *database, int64_t seconds);
static int append_incorrect_time_warning(
    struct mylite_db *database,
    const char *value,
    size_t value_length
);

const char *mylite_timediff_input_kind_name(enum mylite_timediff_input_kind kind) {
    switch (kind) {
    case MYLITE_TIMEDIFF_INPUT_STRING:
        return "string";
    case MYLITE_TIMEDIFF_INPUT_DATE:
        return "date";
    case MYLITE_TIMEDIFF_INPUT_TIME:
        return "time";
    case MYLITE_TIMEDIFF_INPUT_DATETIME:
        return "datetime";
    case MYLITE_TIMEDIFF_INPUT_TIMESTAMP:
        return "timestamp";
    }
    return NULL;
}

bool mylite_timediff_input_kind_from_name(
    const char *name,
    size_t name_length,
    enum mylite_timediff_input_kind *out_kind
) {
    static const struct {
        const char *name;
        enum mylite_timediff_input_kind kind;
    } names[] = {
        {"string", MYLITE_TIMEDIFF_INPUT_STRING},
        {"date", MYLITE_TIMEDIFF_INPUT_DATE},
        {"time", MYLITE_TIMEDIFF_INPUT_TIME},
        {"datetime", MYLITE_TIMEDIFF_INPUT_DATETIME},
        {"timestamp", MYLITE_TIMEDIFF_INPUT_TIMESTAMP},
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

int mylite_timediff_value(
    struct mylite_db *database,
    const char *left_value,
    size_t left_value_length,
    enum mylite_timediff_input_kind left_input_kind,
    bool left_is_null,
    const char *right_value,
    size_t right_value_length,
    enum mylite_timediff_input_kind right_input_kind,
    bool right_is_null,
    char **out_text,
    bool *out_is_null
) {
    struct timediff_value_source left = {
        .value = left_value,
        .value_length = left_value_length,
        .input_kind = left_input_kind,
        .is_null = left_is_null,
    };
    struct timediff_value_source right = {
        .value = right_value,
        .value_length = right_value_length,
        .input_kind = right_input_kind,
        .is_null = right_is_null,
    };

    if (out_text == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_is_null = false;
    return timediff_value_result(database, &left, &right, out_text, out_is_null);
}

int mylite_sqlite_register_timediff_function(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_timediff",
            .argument_count = timediff_sqlite_argument_count,
            .text_representation = SQLITE_UTF8 | SQLITE_DETERMINISTIC,
            .application_data = NULL,
            .scalar_callback = timediff_sqlite_callback,
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

static void timediff_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    enum mylite_timediff_input_kind left_kind = MYLITE_TIMEDIFF_INPUT_STRING;
    enum mylite_timediff_input_kind right_kind = MYLITE_TIMEDIFF_INPUT_STRING;

    if (context == NULL || argc != timediff_sqlite_argument_count || argv == NULL ||
        argv[0] == NULL || argv[1] == NULL || argv[2] == NULL || argv[3] == NULL) {
        sqlite3_result_error(context, "invalid MyLite TIMEDIFF callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[1]) == SQLITE_NULL || sqlite3_value_type(argv[3]) == SQLITE_NULL) {
        sqlite3_result_error(context, "invalid MyLite TIMEDIFF input kind", -1);
        return;
    }
    if (timediff_sqlite_input_kind(context, argv[1], &left_kind) != MYLITE_OK ||
        timediff_sqlite_input_kind(context, argv[3], &right_kind) != MYLITE_OK) {
        return;
    }
    (void)timediff_sqlite_result(context, argv[0], left_kind, argv[2], right_kind);
}

static int timediff_sqlite_input_kind(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_timediff_input_kind *out_kind
) {
    const unsigned char *kind_text = sqlite3_value_text(value);
    int kind_length = sqlite3_value_bytes(value);

    if (kind_text == NULL || kind_length < 0 || out_kind == NULL) {
        sqlite3_result_error_nomem(context);
        return MYLITE_NOMEM;
    }
    if (!mylite_timediff_input_kind_from_name(
            (const char *)kind_text,
            (size_t)kind_length,
            out_kind
        )) {
        sqlite3_result_error(context, "invalid MyLite TIMEDIFF input kind", -1);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int timediff_sqlite_result(
    sqlite3_context *context,
    sqlite3_value *left_value,
    enum mylite_timediff_input_kind left_kind,
    sqlite3_value *right_value,
    enum mylite_timediff_input_kind right_kind
) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    const char *left_text = NULL;
    const char *right_text = NULL;
    size_t left_text_length = 0U;
    size_t right_text_length = 0U;
    bool left_is_null = false;
    bool right_is_null = false;
    bool result_is_null = false;
    char *result = NULL;
    int rc = MYLITE_OK;

    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite TIMEDIFF owner", -1);
        return MYLITE_ERROR;
    }

    rc = sqlite_value_text_pointer(
        context,
        left_value,
        &left_text,
        &left_text_length,
        &left_is_null
    );
    if (rc == MYLITE_OK) {
        rc = sqlite_value_text_pointer(
            context,
            right_value,
            &right_text,
            &right_text_length,
            &right_is_null
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_timediff_value(
        database,
        left_text,
        left_text_length,
        left_kind,
        left_is_null,
        right_text,
        right_text_length,
        right_kind,
        right_is_null,
        &result,
        &result_is_null
    );
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
        } else {
            sqlite3_result_error(context, "MyLite TIMEDIFF failed", -1);
        }
        free(result);
        return rc;
    }
    if (result_is_null) {
        sqlite3_result_null(context);
    } else {
        sqlite3_result_text(context, result, -1, SQLITE_TRANSIENT);
    }
    free(result);
    return MYLITE_OK;
}

static int sqlite_value_text_pointer(
    sqlite3_context *context,
    sqlite3_value *value,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    const unsigned char *text = NULL;
    int text_length = 0;

    if (value == NULL || out_text == NULL || out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;
    if (sqlite3_value_type(value) == SQLITE_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    text = sqlite3_value_text(value);
    text_length = sqlite3_value_bytes(value);
    if (text == NULL || text_length < 0) {
        sqlite3_result_error_nomem(context);
        return MYLITE_NOMEM;
    }
    *out_text = (const char *)text;
    *out_text_length = (size_t)text_length;
    return MYLITE_OK;
}

static int timediff_value_result(
    struct mylite_db *database,
    const struct timediff_value_source *left,
    const struct timediff_value_source *right,
    char **out_text,
    bool *out_is_null
) {
    struct timediff_value_parts left_parts = {0};
    struct timediff_value_parts right_parts = {0};
    int64_t seconds = 0;
    int rc = MYLITE_OK;

    if (out_text == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_is_null = false;

    rc = parse_timediff_argument(database, left, &left_parts);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (left_parts.domain == TIMEDIFF_DOMAIN_NONE) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    rc = parse_timediff_argument(database, right, &right_parts);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (right_parts.domain == TIMEDIFF_DOMAIN_NONE) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    rc = timediff_result_seconds(&left_parts, &right_parts, out_is_null, &seconds);
    if (rc != MYLITE_OK || *out_is_null) {
        return rc;
    }
    return format_timediff_result(database, seconds, out_text);
}

static int parse_timediff_argument(
    struct mylite_db *database,
    const struct timediff_value_source *source,
    struct timediff_value_parts *out_parts
) {
    enum timediff_parse_status status = TIMEDIFF_PARSE_INVALID;

    if (source == NULL || out_parts == NULL) {
        return MYLITE_MISUSE;
    }
    *out_parts = (struct timediff_value_parts){0};
    status = parse_timediff_value(source, out_parts);
    if (status == TIMEDIFF_PARSE_VALID) {
        return MYLITE_OK;
    }
    if (status == TIMEDIFF_PARSE_NULL) {
        out_parts->domain = TIMEDIFF_DOMAIN_NONE;
        return MYLITE_OK;
    }

    out_parts->domain = TIMEDIFF_DOMAIN_NONE;
    return append_incorrect_time_warning(database, source->value, source->value_length);
}

static enum timediff_parse_status parse_timediff_value(
    const struct timediff_value_source *source,
    struct timediff_value_parts *out_parts
) {
    if (source == NULL || out_parts == NULL) {
        return TIMEDIFF_PARSE_INVALID;
    }
    *out_parts = (struct timediff_value_parts){0};
    if (source->is_null) {
        return TIMEDIFF_PARSE_NULL;
    }
    if (source->value == NULL) {
        return TIMEDIFF_PARSE_INVALID;
    }

    switch (source->input_kind) {
    case MYLITE_TIMEDIFF_INPUT_STRING:
        return parse_string_timediff_value(source->value, source->value_length, out_parts);
    case MYLITE_TIMEDIFF_INPUT_DATE:
        return parse_date_timediff_value(source->value, source->value_length, out_parts);
    case MYLITE_TIMEDIFF_INPUT_TIME:
        return parse_time_timediff_value(source->value, source->value_length, out_parts);
    case MYLITE_TIMEDIFF_INPUT_DATETIME:
    case MYLITE_TIMEDIFF_INPUT_TIMESTAMP:
        return parse_datetime_timediff_value(source->value, source->value_length, out_parts);
    }
    return TIMEDIFF_PARSE_INVALID;
}

static enum timediff_parse_status parse_string_timediff_value(
    const char *value,
    size_t value_length,
    struct timediff_value_parts *out_parts
) {
    enum timediff_parse_status status = TIMEDIFF_PARSE_INVALID;

    status = parse_datetime_timediff_value(value, value_length, out_parts);
    if (status == TIMEDIFF_PARSE_VALID) {
        return status;
    }
    return parse_time_timediff_value(value, value_length, out_parts);
}

static enum timediff_parse_status parse_date_timediff_value(
    const char *value,
    size_t value_length,
    struct timediff_value_parts *out_parts
) {
    if (out_parts == NULL) {
        return TIMEDIFF_PARSE_INVALID;
    }
    *out_parts = (struct timediff_value_parts){0};
    if (!parse_date_text(value, value_length, &out_parts->datetime.date)) {
        return TIMEDIFF_PARSE_INVALID;
    }
    out_parts->domain = TIMEDIFF_DOMAIN_DATE;
    return TIMEDIFF_PARSE_VALID;
}

static enum timediff_parse_status parse_datetime_timediff_value(
    const char *value,
    size_t value_length,
    struct timediff_value_parts *out_parts
) {
    if (out_parts == NULL) {
        return TIMEDIFF_PARSE_INVALID;
    }
    *out_parts = (struct timediff_value_parts){0};
    if (value == NULL || value_length != timediff_datetime_text_length ||
        value[timediff_time_separator] != ' ' || value[timediff_first_time_separator] != ':' ||
        value[timediff_second_time_separator] != ':' ||
        !parse_date_text(value, timediff_date_text_length, &out_parts->datetime.date) ||
        !parse_digits(
            value + timediff_hour_offset,
            timediff_two_digit_count,
            &out_parts->datetime.time.hour
        ) ||
        !parse_digits(
            value + timediff_minute_offset,
            timediff_two_digit_count,
            &out_parts->datetime.time.minute
        ) ||
        !parse_digits(
            value + timediff_second_offset,
            timediff_two_digit_count,
            &out_parts->datetime.time.second
        ) ||
        out_parts->datetime.time.hour > timediff_datetime_hour_maximum ||
        out_parts->datetime.time.minute > timediff_minute_second_maximum ||
        out_parts->datetime.time.second > timediff_minute_second_maximum) {
        return TIMEDIFF_PARSE_INVALID;
    }
    out_parts->domain = TIMEDIFF_DOMAIN_DATETIME;
    return TIMEDIFF_PARSE_VALID;
}

static enum timediff_parse_status parse_time_timediff_value(
    const char *value,
    size_t value_length,
    struct timediff_value_parts *out_parts
) {
    struct timediff_time_parts time = {0};
    bool negative = false;

    if (out_parts == NULL) {
        return TIMEDIFF_PARSE_INVALID;
    }
    *out_parts = (struct timediff_value_parts){0};
    if (!parse_time_text(value, value_length, true, &time, &negative)) {
        return TIMEDIFF_PARSE_INVALID;
    }
    out_parts->domain = TIMEDIFF_DOMAIN_TIME;
    out_parts->time_seconds = seconds_of_day(&time);
    if (negative) {
        out_parts->time_seconds = -out_parts->time_seconds;
    }
    return TIMEDIFF_PARSE_VALID;
}

static bool parse_date_text(
    const char *value,
    size_t value_length,
    struct timediff_date_parts *out_date
) {
    if (value == NULL || out_date == NULL || value_length != timediff_date_text_length ||
        value[timediff_first_date_separator] != '-' ||
        value[timediff_second_date_separator] != '-' ||
        !parse_digits(value, timediff_four_digit_count, &out_date->year) ||
        !parse_digits(value + timediff_month_offset, timediff_two_digit_count, &out_date->month) ||
        !parse_digits(value + timediff_day_offset, timediff_two_digit_count, &out_date->day)) {
        return false;
    }
    return date_is_valid(out_date);
}

static bool parse_time_text(
    const char *value,
    size_t value_length,
    bool allow_sign,
    struct timediff_time_parts *out_time,
    bool *out_negative
) {
    size_t offset = 0U;
    size_t unsigned_length = value_length;
    size_t hour_digits = 0U;

    if (value == NULL || out_time == NULL || out_negative == NULL) {
        return false;
    }
    *out_time = (struct timediff_time_parts){0};
    *out_negative = false;
    if (allow_sign && value_length > 0U && value[0] == '-') {
        *out_negative = true;
        offset = 1U;
        --unsigned_length;
    }
    if (unsigned_length < timediff_time_text_minimum_length ||
        unsigned_length > timediff_time_text_maximum_length) {
        return false;
    }

    hour_digits = unsigned_length - timediff_time_suffix_length;
    if (value[offset + hour_digits] != ':' || value[offset + hour_digits + 3U] != ':' ||
        !parse_digits(value + offset, hour_digits, &out_time->hour) ||
        !parse_digits(
            value + offset + hour_digits + 1U,
            timediff_two_digit_count,
            &out_time->minute
        ) ||
        !parse_digits(
            value + offset + hour_digits + 4U,
            timediff_two_digit_count,
            &out_time->second
        ) ||
        out_time->hour > timediff_time_hour_maximum ||
        out_time->minute > timediff_minute_second_maximum ||
        out_time->second > timediff_minute_second_maximum) {
        return false;
    }
    return true;
}

static bool parse_digits(const char *text, size_t length, int *out_value) {
    int value = 0;

    if (text == NULL || length == 0U || out_value == NULL) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
        value = (value * timediff_digit_radix) + (text[index] - '0');
    }
    *out_value = value;
    return true;
}

static bool date_is_valid(const struct timediff_date_parts *date) {
    if (date == NULL || date->year < timediff_date_year_minimum ||
        date->year > timediff_date_year_maximum || date->month < 1 ||
        date->month > timediff_month_maximum || date->day < 1 ||
        date->day > days_in_month(date->year, date->month)) {
        return false;
    }
    return true;
}

static int days_in_month(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == timediff_february && is_leap_year(year)) {
        return timediff_leap_day;
    }
    if (month < 1 || month > timediff_month_maximum) {
        return 0;
    }
    return days[month - 1];
}

static bool is_leap_year(int year) {
    if ((year % timediff_leap_quadrennial_year_cycle) != 0) {
        return false;
    }
    if ((year % timediff_leap_century_year_cycle) != 0) {
        return true;
    }
    return (year % timediff_leap_quadricentennial_year_cycle) == 0;
}

static int day_of_year(const struct timediff_date_parts *date) {
    int day = 0;

    if (date == NULL) {
        return 0;
    }
    for (int month = 1; month < date->month; ++month) {
        day += days_in_month(date->year, month);
    }
    return day + date->day;
}

static int64_t day_number(const struct timediff_date_parts *date) {
    int64_t previous_year = 0;

    if (date == NULL) {
        return 0;
    }
    previous_year = (int64_t)date->year - 1;
    return (previous_year * timediff_days_per_common_year) +
           (previous_year / timediff_leap_quadrennial_year_cycle) -
           (previous_year / timediff_leap_century_year_cycle) +
           (previous_year / timediff_leap_quadricentennial_year_cycle) + day_of_year(date);
}

static int64_t seconds_of_day(const struct timediff_time_parts *time) {
    if (time == NULL) {
        return 0;
    }
    return ((int64_t)time->hour * timediff_seconds_per_hour) +
           ((int64_t)time->minute * timediff_seconds_per_minute) + (int64_t)time->second;
}

static int64_t datetime_second_number(const struct timediff_datetime_parts *datetime) {
    if (datetime == NULL) {
        return 0;
    }
    return (day_number(&datetime->date) * timediff_seconds_per_day) +
           seconds_of_day(&datetime->time);
}

static int timediff_result_seconds(
    const struct timediff_value_parts *left,
    const struct timediff_value_parts *right,
    bool *out_is_null,
    int64_t *out_seconds
) {
    if (left == NULL || right == NULL || out_is_null == NULL || out_seconds == NULL) {
        return MYLITE_MISUSE;
    }
    *out_is_null = false;
    *out_seconds = 0;

    if (left->domain != right->domain) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    switch (left->domain) {
    case TIMEDIFF_DOMAIN_TIME:
        *out_seconds = left->time_seconds - right->time_seconds;
        return MYLITE_OK;
    case TIMEDIFF_DOMAIN_DATE:
    case TIMEDIFF_DOMAIN_DATETIME:
        *out_seconds =
            datetime_second_number(&left->datetime) - datetime_second_number(&right->datetime);
        return MYLITE_OK;
    case TIMEDIFF_DOMAIN_NONE:
    default:
        *out_is_null = true;
        return MYLITE_OK;
    }
}

static int format_timediff_result(struct mylite_db *database, int64_t seconds, char **out_text) {
    int64_t clipped = seconds;
    int rc = MYLITE_OK;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (seconds > timediff_maximum_second) {
        clipped = timediff_maximum_second;
        rc = append_timediff_truncation_warning(database, seconds);
    } else if (seconds < -((int64_t)timediff_maximum_second)) {
        clipped = -((int64_t)timediff_maximum_second);
        rc = append_timediff_truncation_warning(database, seconds);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    return format_time_text(database, clipped, out_text);
}

static int format_time_text(struct mylite_db *database, int64_t seconds, char **out_text) {
    char buffer[timediff_result_capacity];
    bool negative = seconds < 0;
    int64_t magnitude = seconds;
    int64_t hour = 0;
    int64_t minute = 0;
    int64_t second = 0;
    int written = 0;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (negative) {
        magnitude = -magnitude;
    }
    hour = magnitude / timediff_seconds_per_hour;
    magnitude %= timediff_seconds_per_hour;
    minute = magnitude / timediff_seconds_per_minute;
    second = magnitude % timediff_seconds_per_minute;

    written = snprintf(
        buffer,
        sizeof(buffer),
        "%s%02" PRId64 ":%02" PRId64 ":%02" PRId64,
        negative ? "-" : "",
        hour,
        minute,
        second
    );
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_ERROR,
            "HY000",
            "failed to format TIMEDIFF() result"
        );
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

static int append_timediff_truncation_warning(struct mylite_db *database, int64_t seconds) {
    char *value = NULL;
    int rc = format_time_text(database, seconds, &value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = append_incorrect_time_warning(database, value, strlen(value));
    free(value);
    return rc;
}

static int append_incorrect_time_warning(
    struct mylite_db *database,
    const char *value,
    size_t value_length
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Truncated incorrect time value: '%.*s'",
        value_length > timediff_warning_value_preview_length ? timediff_warning_value_preview_length
                                                             : (int)value_length,
        value == NULL ? "" : value
    );
    int rc = MYLITE_OK;

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
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_NOMEM,
            "HY001",
            "out of memory"
        );
    }
    return rc;
}
