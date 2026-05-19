#include "mylite_datediff.h"

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
    mysql_warning_incorrect_datetime_value = 1292,
    datediff_date_text_length = 10,
    datediff_datetime_text_length = 19,
    datediff_first_date_separator = 4,
    datediff_month_offset = 5,
    datediff_second_date_separator = 7,
    datediff_day_offset = 8,
    datediff_time_separator = 10,
    datediff_hour_offset = 11,
    datediff_first_time_separator = 13,
    datediff_minute_offset = 14,
    datediff_second_time_separator = 16,
    datediff_second_offset = 17,
    datediff_two_digit_count = 2,
    datediff_four_digit_count = 4,
    datediff_month_max = 12,
    datediff_day_max = 31,
    datediff_february = 2,
    datediff_leap_day = 29,
    datediff_hour_max = 23,
    datediff_minute_second_max = 59,
    datediff_result_capacity = 32,
    datediff_digit_radix = 10,
    datediff_leap_quadrennial_year_cycle = 4,
    datediff_leap_century_year_cycle = 100,
    datediff_leap_quadricentennial_year_cycle = 400,
    datediff_days_per_common_year = 365,
};

struct datediff_date_parts {
    int year;
    int month;
    int day;
};

struct datediff_time_parts {
    int hour;
    int minute;
    int second;
};

struct datediff_datetime_parts {
    struct datediff_date_parts date;
    struct datediff_time_parts time;
};

struct datediff_value_source {
    const char *value;
    size_t value_length;
    enum mylite_datediff_input_kind input_kind;
    bool is_null;
};

enum datediff_parse_status {
    DATEDIFF_PARSE_VALID = 0,
    DATEDIFF_PARSE_NULL = 1,
    DATEDIFF_PARSE_INVALID = 2,
    DATEDIFF_PARSE_ZERO_DATE = 3,
};

static void datediff_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static int datediff_sqlite_input_kind(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_datediff_input_kind *out_kind
);
static int datediff_sqlite_result(
    sqlite3_context *context,
    sqlite3_value *left_value,
    enum mylite_datediff_input_kind left_kind,
    sqlite3_value *right_value,
    enum mylite_datediff_input_kind right_kind
);
static int sqlite_value_text_pointer(
    sqlite3_context *context,
    sqlite3_value *value,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);

static int datediff_value_result(
    struct mylite_db *database,
    const struct datediff_value_source *left,
    const struct datediff_value_source *right,
    char **out_text,
    bool *out_is_null
);
static int parse_datediff_argument(
    struct mylite_db *database,
    const struct datediff_value_source *source,
    struct datediff_date_parts *out_date,
    bool *out_is_null_or_invalid
);
static enum datediff_parse_status parse_datediff_value(
    const struct datediff_value_source *source,
    struct datediff_date_parts *out_date
);
static enum datediff_parse_status parse_string_datediff_value(
    const char *value,
    size_t value_length,
    struct datediff_date_parts *out_date
);
static enum datediff_parse_status parse_datetime_text(
    const char *value,
    size_t value_length,
    struct datediff_datetime_parts *out_datetime
);
static enum datediff_parse_status parse_date_text(
    const char *value,
    size_t value_length,
    struct datediff_date_parts *out_date
);
static bool parse_time_text(
    const char *value,
    size_t value_length,
    struct datediff_time_parts *out_time
);
static bool parse_digits(const char *text, size_t length, int *out_value);
static bool date_is_valid(const struct datediff_date_parts *date);
static bool date_is_full_zero(const struct datediff_date_parts *date);
static bool date_is_partial_zero(const struct datediff_date_parts *date);
static bool time_is_valid(const struct datediff_time_parts *time);
static int days_in_month(int year, int month);
static bool is_leap_year(int year);
static int day_of_year(const struct datediff_date_parts *date);
static int64_t day_number(const struct datediff_date_parts *date);
static int format_datediff_result(struct mylite_db *database, int64_t value, char **out_text);
static int append_incorrect_datetime_warning(
    struct mylite_db *database,
    const char *value,
    size_t value_length
);

const char *mylite_datediff_input_kind_name(enum mylite_datediff_input_kind kind) {
    switch (kind) {
    case MYLITE_DATEDIFF_INPUT_STRING:
        return "string";
    case MYLITE_DATEDIFF_INPUT_DATE:
        return "date";
    case MYLITE_DATEDIFF_INPUT_DATETIME:
        return "datetime";
    case MYLITE_DATEDIFF_INPUT_TIMESTAMP:
        return "timestamp";
    }
    return NULL;
}

bool mylite_datediff_input_kind_from_name(
    const char *name,
    size_t name_length,
    enum mylite_datediff_input_kind *out_kind
) {
    static const struct {
        const char *name;
        enum mylite_datediff_input_kind kind;
    } names[] = {
        {"string", MYLITE_DATEDIFF_INPUT_STRING},
        {"date", MYLITE_DATEDIFF_INPUT_DATE},
        {"datetime", MYLITE_DATEDIFF_INPUT_DATETIME},
        {"timestamp", MYLITE_DATEDIFF_INPUT_TIMESTAMP},
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

int mylite_datediff_value(
    struct mylite_db *database,
    const char *left_value,
    size_t left_value_length,
    enum mylite_datediff_input_kind left_input_kind,
    bool left_is_null,
    const char *right_value,
    size_t right_value_length,
    enum mylite_datediff_input_kind right_input_kind,
    bool right_is_null,
    char **out_text,
    bool *out_is_null
) {
    const struct datediff_value_source left = {
        .value = left_value,
        .value_length = left_value_length,
        .input_kind = left_input_kind,
        .is_null = left_is_null,
    };
    const struct datediff_value_source right = {
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
    return datediff_value_result(database, &left, &right, out_text, out_is_null);
}

int mylite_sqlite_register_datediff_function(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_datediff",
            .argument_count = 4,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = datediff_sqlite_callback,
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

static void datediff_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    enum mylite_datediff_input_kind left_kind = MYLITE_DATEDIFF_INPUT_STRING;
    enum mylite_datediff_input_kind right_kind = MYLITE_DATEDIFF_INPUT_STRING;

    if (context == NULL || argc != 4 || argv == NULL || argv[0] == NULL || argv[1] == NULL ||
        argv[2] == NULL || argv[3] == NULL) {
        sqlite3_result_error(context, "invalid MyLite DATEDIFF callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[1]) == SQLITE_NULL || sqlite3_value_type(argv[3]) == SQLITE_NULL) {
        sqlite3_result_error(context, "invalid MyLite DATEDIFF input kind", -1);
        return;
    }
    if (datediff_sqlite_input_kind(context, argv[1], &left_kind) != MYLITE_OK ||
        datediff_sqlite_input_kind(context, argv[3], &right_kind) != MYLITE_OK) {
        return;
    }
    if (datediff_sqlite_result(context, argv[0], left_kind, argv[2], right_kind) != MYLITE_OK) {
        return;
    }
}

static int datediff_sqlite_input_kind(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_datediff_input_kind *out_kind
) {
    const unsigned char *kind_text = sqlite3_value_text(value);
    int kind_length = sqlite3_value_bytes(value);

    if (kind_text == NULL || kind_length < 0 || out_kind == NULL) {
        sqlite3_result_error_nomem(context);
        return MYLITE_NOMEM;
    }
    if (!mylite_datediff_input_kind_from_name(
            (const char *)kind_text,
            (size_t)kind_length,
            out_kind
        )) {
        sqlite3_result_error(context, "invalid MyLite DATEDIFF input kind", -1);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int datediff_sqlite_result(
    sqlite3_context *context,
    sqlite3_value *left_value,
    enum mylite_datediff_input_kind left_kind,
    sqlite3_value *right_value,
    enum mylite_datediff_input_kind right_kind
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
        sqlite3_result_error(context, "missing MyLite DATEDIFF owner", -1);
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

    rc = mylite_datediff_value(
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
            sqlite3_result_error(context, "MyLite DATEDIFF failed", -1);
        }
        free(result);
        return rc;
    }
    if (result_is_null) {
        sqlite3_result_null(context);
    } else {
        sqlite3_result_int64(context, (sqlite3_int64)strtoll(result, NULL, datediff_digit_radix));
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

static int datediff_value_result(
    struct mylite_db *database,
    const struct datediff_value_source *left,
    const struct datediff_value_source *right,
    char **out_text,
    bool *out_is_null
) {
    struct datediff_date_parts left_date = {0};
    struct datediff_date_parts right_date = {0};
    bool left_is_null_or_invalid = false;
    bool right_is_null_or_invalid = false;
    int rc = MYLITE_OK;

    rc = parse_datediff_argument(database, left, &left_date, &left_is_null_or_invalid);
    if (rc == MYLITE_OK) {
        rc = parse_datediff_argument(database, right, &right_date, &right_is_null_or_invalid);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (left_is_null_or_invalid || right_is_null_or_invalid) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    return format_datediff_result(
        database,
        day_number(&left_date) - day_number(&right_date),
        out_text
    );
}

static int parse_datediff_argument(
    struct mylite_db *database,
    const struct datediff_value_source *source,
    struct datediff_date_parts *out_date,
    bool *out_is_null_or_invalid
) {
    enum datediff_parse_status status = DATEDIFF_PARSE_INVALID;
    int rc = MYLITE_OK;

    if (out_is_null_or_invalid == NULL) {
        return MYLITE_MISUSE;
    }
    *out_is_null_or_invalid = false;
    status = parse_datediff_value(source, out_date);
    if (status == DATEDIFF_PARSE_VALID) {
        return MYLITE_OK;
    }
    *out_is_null_or_invalid = true;
    if (status == DATEDIFF_PARSE_NULL) {
        return MYLITE_OK;
    }
    if (status == DATEDIFF_PARSE_ZERO_DATE && source != NULL &&
        source->input_kind != MYLITE_DATEDIFF_INPUT_STRING) {
        return MYLITE_OK;
    }
    rc = append_incorrect_datetime_warning(
        database,
        source == NULL ? NULL : source->value,
        source == NULL ? 0U : source->value_length
    );
    return rc;
}

static enum datediff_parse_status parse_datediff_value(
    const struct datediff_value_source *source,
    struct datediff_date_parts *out_date
) {
    if (out_date == NULL) {
        return DATEDIFF_PARSE_INVALID;
    }
    *out_date = (struct datediff_date_parts){0};
    if (source == NULL || source->is_null) {
        return DATEDIFF_PARSE_NULL;
    }
    if (source->value == NULL) {
        return DATEDIFF_PARSE_INVALID;
    }

    switch (source->input_kind) {
    case MYLITE_DATEDIFF_INPUT_STRING:
        return parse_string_datediff_value(source->value, source->value_length, out_date);
    case MYLITE_DATEDIFF_INPUT_DATE:
        return parse_date_text(source->value, source->value_length, out_date);
    case MYLITE_DATEDIFF_INPUT_DATETIME:
    case MYLITE_DATEDIFF_INPUT_TIMESTAMP: {
        struct datediff_datetime_parts datetime = {0};
        enum datediff_parse_status status =
            parse_datetime_text(source->value, source->value_length, &datetime);

        if (status == DATEDIFF_PARSE_VALID) {
            *out_date = datetime.date;
        }
        return status;
    }
    }
    return DATEDIFF_PARSE_INVALID;
}

static enum datediff_parse_status parse_string_datediff_value(
    const char *value,
    size_t value_length,
    struct datediff_date_parts *out_date
) {
    struct datediff_datetime_parts datetime = {0};
    enum datediff_parse_status status = parse_date_text(value, value_length, out_date);

    if (status != DATEDIFF_PARSE_INVALID) {
        return status;
    }
    status = parse_datetime_text(value, value_length, &datetime);
    if (status == DATEDIFF_PARSE_VALID) {
        *out_date = datetime.date;
    }
    return status;
}

static enum datediff_parse_status parse_datetime_text(
    const char *value,
    size_t value_length,
    struct datediff_datetime_parts *out_datetime
) {
    enum datediff_parse_status date_status = DATEDIFF_PARSE_INVALID;

    if (value == NULL || out_datetime == NULL || value_length != datediff_datetime_text_length ||
        value[datediff_time_separator] != ' ') {
        return DATEDIFF_PARSE_INVALID;
    }
    date_status = parse_date_text(value, datediff_date_text_length, &out_datetime->date);
    if (date_status != DATEDIFF_PARSE_VALID) {
        return date_status;
    }
    if (!parse_time_text(
            value + datediff_hour_offset,
            datediff_datetime_text_length - datediff_hour_offset,
            &out_datetime->time
        )) {
        return DATEDIFF_PARSE_INVALID;
    }
    return DATEDIFF_PARSE_VALID;
}

static enum datediff_parse_status parse_date_text(
    const char *value,
    size_t value_length,
    struct datediff_date_parts *out_date
) {
    struct datediff_date_parts date = {0};

    if (value == NULL || out_date == NULL || value_length != datediff_date_text_length ||
        value[datediff_first_date_separator] != '-' ||
        value[datediff_second_date_separator] != '-') {
        return DATEDIFF_PARSE_INVALID;
    }
    if (!parse_digits(value, datediff_four_digit_count, &date.year) ||
        !parse_digits(value + datediff_month_offset, datediff_two_digit_count, &date.month) ||
        !parse_digits(value + datediff_day_offset, datediff_two_digit_count, &date.day)) {
        return DATEDIFF_PARSE_INVALID;
    }
    *out_date = date;
    if (date_is_full_zero(&date) || date_is_partial_zero(&date)) {
        return DATEDIFF_PARSE_ZERO_DATE;
    }
    if (date_is_valid(&date)) {
        return DATEDIFF_PARSE_VALID;
    }
    return DATEDIFF_PARSE_INVALID;
}

static bool parse_time_text(
    const char *value,
    size_t value_length,
    struct datediff_time_parts *out_time
) {
    struct datediff_time_parts time = {0};

    if (value == NULL || out_time == NULL ||
        value_length != datediff_datetime_text_length - datediff_hour_offset ||
        value[datediff_first_time_separator - datediff_hour_offset] != ':' ||
        value[datediff_second_time_separator - datediff_hour_offset] != ':') {
        return false;
    }
    if (!parse_digits(value, datediff_two_digit_count, &time.hour) ||
        !parse_digits(
            value + datediff_minute_offset - datediff_hour_offset,
            datediff_two_digit_count,
            &time.minute
        ) ||
        !parse_digits(
            value + datediff_second_offset - datediff_hour_offset,
            datediff_two_digit_count,
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
        value = (value * datediff_digit_radix) + (text[index] - '0');
    }
    *out_value = value;
    return true;
}

static bool date_is_valid(const struct datediff_date_parts *date) {
    if (date == NULL || date->month < 1 || date->month > datediff_month_max || date->day < 1 ||
        date->day > datediff_day_max) {
        return false;
    }
    return date->day <= days_in_month(date->year, date->month);
}

static bool date_is_full_zero(const struct datediff_date_parts *date) {
    if (date == NULL) {
        return false;
    }
    return (date->year == 0 && date->month == 0 && date->day == 0) != 0;
}

static bool date_is_partial_zero(const struct datediff_date_parts *date) {
    if (date == NULL || date_is_full_zero(date)) {
        return false;
    }
    return (date->month == 0 || date->day == 0) != 0;
}

static bool time_is_valid(const struct datediff_time_parts *time) {
    if (time == NULL) {
        return false;
    }
    return (time->hour >= 0 && time->hour <= datediff_hour_max && time->minute >= 0 &&
            time->minute <= datediff_minute_second_max && time->second >= 0 &&
            time->second <= datediff_minute_second_max) != 0;
}

static int days_in_month(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == datediff_february && is_leap_year(year)) {
        return datediff_leap_day;
    }
    if (month < 1 || month > datediff_month_max) {
        return 0;
    }
    return days[month - 1];
}

static bool is_leap_year(int year) {
    if (year == 0) {
        return false;
    }
    return ((year % datediff_leap_quadrennial_year_cycle == 0 &&
             year % datediff_leap_century_year_cycle != 0) ||
            year % datediff_leap_quadricentennial_year_cycle == 0) != 0;
}

static int day_of_year(const struct datediff_date_parts *date) {
    int result = 0;

    if (date == NULL) {
        return 0;
    }
    for (int month = 1; month < date->month; ++month) {
        result += days_in_month(date->year, month);
    }
    return result + date->day;
}

static int64_t day_number(const struct datediff_date_parts *date) {
    int64_t year = date == NULL ? 0 : date->year;
    int64_t years_before = year <= 0 ? 0 : year - 1;
    int64_t leap_days_before_year = (years_before / datediff_leap_quadrennial_year_cycle) -
                                    (years_before / datediff_leap_century_year_cycle) +
                                    (years_before / datediff_leap_quadricentennial_year_cycle);

    if (year > 0) {
        return (year * datediff_days_per_common_year) + leap_days_before_year + day_of_year(date);
    }
    return day_of_year(date);
}

static int format_datediff_result(struct mylite_db *database, int64_t value, char **out_text) {
    char buffer[datediff_result_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%" PRId64, value);

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
