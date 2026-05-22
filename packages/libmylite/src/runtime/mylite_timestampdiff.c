#include "mylite_timestampdiff.h"

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
    timestampdiff_date_text_length = 10,
    timestampdiff_datetime_text_length = 19,
    timestampdiff_first_date_separator = 4,
    timestampdiff_month_offset = 5,
    timestampdiff_second_date_separator = 7,
    timestampdiff_day_offset = 8,
    timestampdiff_time_separator = 10,
    timestampdiff_hour_offset = 11,
    timestampdiff_first_time_separator = 13,
    timestampdiff_minute_offset = 14,
    timestampdiff_second_time_separator = 16,
    timestampdiff_second_offset = 17,
    timestampdiff_two_digit_count = 2,
    timestampdiff_four_digit_count = 4,
    timestampdiff_month_max = 12,
    timestampdiff_day_max = 31,
    timestampdiff_february = 2,
    timestampdiff_leap_day = 29,
    timestampdiff_hour_max = 23,
    timestampdiff_minute_second_max = 59,
    timestampdiff_result_capacity = 32,
    timestampdiff_digit_radix = 10,
    timestampdiff_leap_quadrennial_year_cycle = 4,
    timestampdiff_leap_century_year_cycle = 100,
    timestampdiff_leap_quadricentennial_year_cycle = 400,
    timestampdiff_days_per_common_year = 365,
    timestampdiff_seconds_per_minute = 60,
    timestampdiff_seconds_per_hour = 3600,
    timestampdiff_seconds_per_day = 86400,
    timestampdiff_seconds_per_week = 604800,
    timestampdiff_sqlite_argument_count = 5,
};

struct timestampdiff_date_parts {
    int year;
    int month;
    int day;
};

struct timestampdiff_time_parts {
    int hour;
    int minute;
    int second;
};

struct timestampdiff_datetime_parts {
    struct timestampdiff_date_parts date;
    struct timestampdiff_time_parts time;
};

struct timestampdiff_value_source {
    const char *value;
    size_t value_length;
    enum mylite_timestampdiff_input_kind input_kind;
    bool is_null;
};

enum timestampdiff_parse_status {
    TIMESTAMPDIFF_PARSE_VALID = 0,
    TIMESTAMPDIFF_PARSE_NULL = 1,
    TIMESTAMPDIFF_PARSE_INVALID = 2,
    TIMESTAMPDIFF_PARSE_ZERO_DATE = 3,
};

static void timestampdiff_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static int timestampdiff_sqlite_unit(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_timestampdiff_unit *out_unit
);
static int timestampdiff_sqlite_input_kind(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_timestampdiff_input_kind *out_kind
);
static int timestampdiff_sqlite_result(
    sqlite3_context *context,
    enum mylite_timestampdiff_unit unit,
    sqlite3_value *left_value,
    enum mylite_timestampdiff_input_kind left_kind,
    sqlite3_value *right_value,
    enum mylite_timestampdiff_input_kind right_kind
);
static int sqlite_value_text_pointer(
    sqlite3_context *context,
    sqlite3_value *value,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);

static int timestampdiff_value_result(
    struct mylite_db *database,
    enum mylite_timestampdiff_unit unit,
    const struct timestampdiff_value_source *left,
    const struct timestampdiff_value_source *right,
    char **out_text,
    bool *out_is_null
);
static int parse_timestampdiff_argument(
    struct mylite_db *database,
    const struct timestampdiff_value_source *source,
    struct timestampdiff_datetime_parts *out_datetime,
    bool *out_is_null_or_invalid
);
static enum timestampdiff_parse_status parse_timestampdiff_value(
    const struct timestampdiff_value_source *source,
    struct timestampdiff_datetime_parts *out_datetime
);
static enum timestampdiff_parse_status parse_string_timestampdiff_value(
    const char *value,
    size_t value_length,
    struct timestampdiff_datetime_parts *out_datetime
);
static enum timestampdiff_parse_status parse_datetime_text(
    const char *value,
    size_t value_length,
    struct timestampdiff_datetime_parts *out_datetime
);
static enum timestampdiff_parse_status parse_date_text(
    const char *value,
    size_t value_length,
    struct timestampdiff_date_parts *out_date
);
static bool parse_time_text(
    const char *value,
    size_t value_length,
    struct timestampdiff_time_parts *out_time
);
static bool parse_digits(const char *text, size_t length, int *out_value);
static bool date_is_valid(const struct timestampdiff_date_parts *date);
static bool date_is_full_zero(const struct timestampdiff_date_parts *date);
static bool date_is_partial_zero(const struct timestampdiff_date_parts *date);
static bool time_is_valid(const struct timestampdiff_time_parts *time);
static int days_in_month(int year, int month);
static bool is_leap_year(int year);
static int day_of_year(const struct timestampdiff_date_parts *date);
static int64_t day_number(const struct timestampdiff_date_parts *date);
static int seconds_of_day(const struct timestampdiff_time_parts *time);
static int compare_day_time(
    const struct timestampdiff_datetime_parts *left,
    const struct timestampdiff_datetime_parts *right
);
static int64_t whole_month_difference(
    const struct timestampdiff_datetime_parts *left,
    const struct timestampdiff_datetime_parts *right
);
static int64_t elapsed_second_difference(
    const struct timestampdiff_datetime_parts *left,
    const struct timestampdiff_datetime_parts *right
);
static int64_t timestampdiff_result(
    enum mylite_timestampdiff_unit unit,
    const struct timestampdiff_datetime_parts *left,
    const struct timestampdiff_datetime_parts *right
);
static int format_timestampdiff_result(struct mylite_db *database, int64_t value, char **out_text);
static int append_incorrect_datetime_warning(
    struct mylite_db *database,
    const char *value,
    size_t value_length
);

const char *mylite_timestampdiff_unit_name(enum mylite_timestampdiff_unit unit) {
    switch (unit) {
    case MYLITE_TIMESTAMPDIFF_UNIT_YEAR:
        return "year";
    case MYLITE_TIMESTAMPDIFF_UNIT_QUARTER:
        return "quarter";
    case MYLITE_TIMESTAMPDIFF_UNIT_MONTH:
        return "month";
    case MYLITE_TIMESTAMPDIFF_UNIT_WEEK:
        return "week";
    case MYLITE_TIMESTAMPDIFF_UNIT_DAY:
        return "day";
    case MYLITE_TIMESTAMPDIFF_UNIT_HOUR:
        return "hour";
    case MYLITE_TIMESTAMPDIFF_UNIT_MINUTE:
        return "minute";
    case MYLITE_TIMESTAMPDIFF_UNIT_SECOND:
        return "second";
    case MYLITE_TIMESTAMPDIFF_UNIT_MICROSECOND:
        return "microsecond";
    }
    return NULL;
}

bool mylite_timestampdiff_unit_from_name(
    const char *name,
    size_t name_length,
    enum mylite_timestampdiff_unit *out_unit
) {
    static const struct {
        const char *name;
        enum mylite_timestampdiff_unit unit;
    } names[] = {
        {"year", MYLITE_TIMESTAMPDIFF_UNIT_YEAR},
        {"quarter", MYLITE_TIMESTAMPDIFF_UNIT_QUARTER},
        {"month", MYLITE_TIMESTAMPDIFF_UNIT_MONTH},
        {"week", MYLITE_TIMESTAMPDIFF_UNIT_WEEK},
        {"day", MYLITE_TIMESTAMPDIFF_UNIT_DAY},
        {"hour", MYLITE_TIMESTAMPDIFF_UNIT_HOUR},
        {"minute", MYLITE_TIMESTAMPDIFF_UNIT_MINUTE},
        {"second", MYLITE_TIMESTAMPDIFF_UNIT_SECOND},
        {"microsecond", MYLITE_TIMESTAMPDIFF_UNIT_MICROSECOND},
    };

    if (name == NULL || out_unit == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        size_t current_length = strlen(names[index].name);

        if (name_length == current_length && memcmp(name, names[index].name, name_length) == 0) {
            *out_unit = names[index].unit;
            return true;
        }
    }
    return false;
}

const char *mylite_timestampdiff_input_kind_name(enum mylite_timestampdiff_input_kind kind) {
    switch (kind) {
    case MYLITE_TIMESTAMPDIFF_INPUT_STRING:
        return "string";
    case MYLITE_TIMESTAMPDIFF_INPUT_DATE:
        return "date";
    case MYLITE_TIMESTAMPDIFF_INPUT_DATETIME:
        return "datetime";
    case MYLITE_TIMESTAMPDIFF_INPUT_TIMESTAMP:
        return "timestamp";
    }
    return NULL;
}

bool mylite_timestampdiff_input_kind_from_name(
    const char *name,
    size_t name_length,
    enum mylite_timestampdiff_input_kind *out_kind
) {
    static const struct {
        const char *name;
        enum mylite_timestampdiff_input_kind kind;
    } names[] = {
        {"string", MYLITE_TIMESTAMPDIFF_INPUT_STRING},
        {"date", MYLITE_TIMESTAMPDIFF_INPUT_DATE},
        {"datetime", MYLITE_TIMESTAMPDIFF_INPUT_DATETIME},
        {"timestamp", MYLITE_TIMESTAMPDIFF_INPUT_TIMESTAMP},
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

int mylite_timestampdiff_value(
    struct mylite_db *database,
    enum mylite_timestampdiff_unit unit,
    const char *left_value,
    size_t left_value_length,
    enum mylite_timestampdiff_input_kind left_input_kind,
    bool left_is_null,
    const char *right_value,
    size_t right_value_length,
    enum mylite_timestampdiff_input_kind right_input_kind,
    bool right_is_null,
    char **out_text,
    bool *out_is_null
) {
    const struct timestampdiff_value_source left = {
        .value = left_value,
        .value_length = left_value_length,
        .input_kind = left_input_kind,
        .is_null = left_is_null,
    };
    const struct timestampdiff_value_source right = {
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
    return timestampdiff_value_result(database, unit, &left, &right, out_text, out_is_null);
}

int mylite_sqlite_register_timestampdiff_function(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_timestampdiff",
            .argument_count = timestampdiff_sqlite_argument_count,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = timestampdiff_sqlite_callback,
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

static void timestampdiff_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    enum mylite_timestampdiff_unit unit = MYLITE_TIMESTAMPDIFF_UNIT_DAY;
    enum mylite_timestampdiff_input_kind left_kind = MYLITE_TIMESTAMPDIFF_INPUT_STRING;
    enum mylite_timestampdiff_input_kind right_kind = MYLITE_TIMESTAMPDIFF_INPUT_STRING;

    if (context == NULL || argc != timestampdiff_sqlite_argument_count || argv == NULL ||
        argv[0] == NULL || argv[1] == NULL || argv[2] == NULL || argv[3] == NULL ||
        argv[4] == NULL) {
        sqlite3_result_error(context, "invalid MyLite TIMESTAMPDIFF callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[2]) == SQLITE_NULL ||
        sqlite3_value_type(argv[4]) == SQLITE_NULL) {
        sqlite3_result_error(context, "invalid MyLite TIMESTAMPDIFF discriminator", -1);
        return;
    }
    if (timestampdiff_sqlite_unit(context, argv[0], &unit) != MYLITE_OK ||
        timestampdiff_sqlite_input_kind(context, argv[2], &left_kind) != MYLITE_OK ||
        timestampdiff_sqlite_input_kind(context, argv[4], &right_kind) != MYLITE_OK) {
        return;
    }
    if (timestampdiff_sqlite_result(context, unit, argv[1], left_kind, argv[3], right_kind) !=
        MYLITE_OK) {
        return;
    }
}

static int timestampdiff_sqlite_unit(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_timestampdiff_unit *out_unit
) {
    const unsigned char *unit_text = sqlite3_value_text(value);
    int unit_length = sqlite3_value_bytes(value);

    if (unit_text == NULL || unit_length < 0 || out_unit == NULL) {
        sqlite3_result_error_nomem(context);
        return MYLITE_NOMEM;
    }
    if (!mylite_timestampdiff_unit_from_name(
            (const char *)unit_text,
            (size_t)unit_length,
            out_unit
        )) {
        sqlite3_result_error(context, "invalid MyLite TIMESTAMPDIFF unit", -1);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int timestampdiff_sqlite_input_kind(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_timestampdiff_input_kind *out_kind
) {
    const unsigned char *kind_text = sqlite3_value_text(value);
    int kind_length = sqlite3_value_bytes(value);

    if (kind_text == NULL || kind_length < 0 || out_kind == NULL) {
        sqlite3_result_error_nomem(context);
        return MYLITE_NOMEM;
    }
    if (!mylite_timestampdiff_input_kind_from_name(
            (const char *)kind_text,
            (size_t)kind_length,
            out_kind
        )) {
        sqlite3_result_error(context, "invalid MyLite TIMESTAMPDIFF input kind", -1);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int timestampdiff_sqlite_result(
    sqlite3_context *context,
    enum mylite_timestampdiff_unit unit,
    sqlite3_value *left_value,
    enum mylite_timestampdiff_input_kind left_kind,
    sqlite3_value *right_value,
    enum mylite_timestampdiff_input_kind right_kind
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
        sqlite3_result_error(context, "missing MyLite TIMESTAMPDIFF owner", -1);
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

    rc = mylite_timestampdiff_value(
        database,
        unit,
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
            sqlite3_result_error(context, "MyLite TIMESTAMPDIFF failed", -1);
        }
        free(result);
        return rc;
    }

    if (result_is_null) {
        sqlite3_result_null(context);
    } else {
        sqlite3_result_int64(
            context,
            (sqlite3_int64)strtoll(result, NULL, timestampdiff_digit_radix)
        );
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

static int timestampdiff_value_result(
    struct mylite_db *database,
    enum mylite_timestampdiff_unit unit,
    const struct timestampdiff_value_source *left,
    const struct timestampdiff_value_source *right,
    char **out_text,
    bool *out_is_null
) {
    struct timestampdiff_datetime_parts left_datetime = {0};
    struct timestampdiff_datetime_parts right_datetime = {0};
    bool is_null_or_invalid = false;
    int rc = MYLITE_OK;

    if (unit == MYLITE_TIMESTAMPDIFF_UNIT_MICROSECOND) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_ERROR,
            "HY000",
            "TIMESTAMPDIFF() does not yet support MICROSECOND"
        );
        return MYLITE_ERROR;
    }

    rc = parse_timestampdiff_argument(database, left, &left_datetime, &is_null_or_invalid);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_null_or_invalid) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    rc = parse_timestampdiff_argument(database, right, &right_datetime, &is_null_or_invalid);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_null_or_invalid) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    return format_timestampdiff_result(
        database,
        timestampdiff_result(unit, &left_datetime, &right_datetime),
        out_text
    );
}

static int parse_timestampdiff_argument(
    struct mylite_db *database,
    const struct timestampdiff_value_source *source,
    struct timestampdiff_datetime_parts *out_datetime,
    bool *out_is_null_or_invalid
) {
    enum timestampdiff_parse_status status = TIMESTAMPDIFF_PARSE_INVALID;
    int rc = MYLITE_OK;

    if (out_is_null_or_invalid == NULL) {
        return MYLITE_MISUSE;
    }
    *out_is_null_or_invalid = false;
    status = parse_timestampdiff_value(source, out_datetime);
    if (status == TIMESTAMPDIFF_PARSE_VALID) {
        return MYLITE_OK;
    }
    *out_is_null_or_invalid = true;
    if (status == TIMESTAMPDIFF_PARSE_NULL) {
        return MYLITE_OK;
    }
    if (status == TIMESTAMPDIFF_PARSE_ZERO_DATE && source != NULL &&
        source->input_kind != MYLITE_TIMESTAMPDIFF_INPUT_STRING) {
        return MYLITE_OK;
    }
    rc = append_incorrect_datetime_warning(
        database,
        source == NULL ? NULL : source->value,
        source == NULL ? 0U : source->value_length
    );
    return rc;
}

static enum timestampdiff_parse_status parse_timestampdiff_value(
    const struct timestampdiff_value_source *source,
    struct timestampdiff_datetime_parts *out_datetime
) {
    enum timestampdiff_parse_status status = TIMESTAMPDIFF_PARSE_INVALID;

    if (out_datetime == NULL) {
        return TIMESTAMPDIFF_PARSE_INVALID;
    }
    *out_datetime = (struct timestampdiff_datetime_parts){0};
    if (source == NULL || source->is_null) {
        return TIMESTAMPDIFF_PARSE_NULL;
    }
    if (source->value == NULL) {
        return TIMESTAMPDIFF_PARSE_INVALID;
    }

    switch (source->input_kind) {
    case MYLITE_TIMESTAMPDIFF_INPUT_STRING:
        return parse_string_timestampdiff_value(source->value, source->value_length, out_datetime);
    case MYLITE_TIMESTAMPDIFF_INPUT_DATE:
        status = parse_date_text(source->value, source->value_length, &out_datetime->date);
        out_datetime->time = (struct timestampdiff_time_parts){0};
        return status;
    case MYLITE_TIMESTAMPDIFF_INPUT_DATETIME:
    case MYLITE_TIMESTAMPDIFF_INPUT_TIMESTAMP:
        return parse_datetime_text(source->value, source->value_length, out_datetime);
    }
    return TIMESTAMPDIFF_PARSE_INVALID;
}

static enum timestampdiff_parse_status parse_string_timestampdiff_value(
    const char *value,
    size_t value_length,
    struct timestampdiff_datetime_parts *out_datetime
) {
    enum timestampdiff_parse_status status =
        parse_date_text(value, value_length, &out_datetime->date);

    if (status != TIMESTAMPDIFF_PARSE_INVALID) {
        out_datetime->time = (struct timestampdiff_time_parts){0};
        return status;
    }
    return parse_datetime_text(value, value_length, out_datetime);
}

static enum timestampdiff_parse_status parse_datetime_text(
    const char *value,
    size_t value_length,
    struct timestampdiff_datetime_parts *out_datetime
) {
    enum timestampdiff_parse_status date_status = TIMESTAMPDIFF_PARSE_INVALID;

    if (value == NULL || out_datetime == NULL ||
        value_length != timestampdiff_datetime_text_length ||
        value[timestampdiff_time_separator] != ' ') {
        return TIMESTAMPDIFF_PARSE_INVALID;
    }
    date_status = parse_date_text(value, timestampdiff_date_text_length, &out_datetime->date);
    if (date_status != TIMESTAMPDIFF_PARSE_VALID) {
        return date_status;
    }
    if (!parse_time_text(
            value + timestampdiff_hour_offset,
            timestampdiff_datetime_text_length - timestampdiff_hour_offset,
            &out_datetime->time
        )) {
        return TIMESTAMPDIFF_PARSE_INVALID;
    }
    return TIMESTAMPDIFF_PARSE_VALID;
}

static enum timestampdiff_parse_status parse_date_text(
    const char *value,
    size_t value_length,
    struct timestampdiff_date_parts *out_date
) {
    struct timestampdiff_date_parts date = {0};

    if (value == NULL || out_date == NULL || value_length != timestampdiff_date_text_length ||
        value[timestampdiff_first_date_separator] != '-' ||
        value[timestampdiff_second_date_separator] != '-') {
        return TIMESTAMPDIFF_PARSE_INVALID;
    }
    if (!parse_digits(value, timestampdiff_four_digit_count, &date.year) ||
        !parse_digits(
            value + timestampdiff_month_offset,
            timestampdiff_two_digit_count,
            &date.month
        ) ||
        !parse_digits(value + timestampdiff_day_offset, timestampdiff_two_digit_count, &date.day)) {
        return TIMESTAMPDIFF_PARSE_INVALID;
    }
    *out_date = date;
    if (date_is_full_zero(&date) || date_is_partial_zero(&date)) {
        return TIMESTAMPDIFF_PARSE_ZERO_DATE;
    }
    if (date_is_valid(&date)) {
        return TIMESTAMPDIFF_PARSE_VALID;
    }
    return TIMESTAMPDIFF_PARSE_INVALID;
}

static bool parse_time_text(
    const char *value,
    size_t value_length,
    struct timestampdiff_time_parts *out_time
) {
    struct timestampdiff_time_parts time = {0};

    if (value == NULL || out_time == NULL ||
        value_length != timestampdiff_datetime_text_length - timestampdiff_hour_offset ||
        value[timestampdiff_first_time_separator - timestampdiff_hour_offset] != ':' ||
        value[timestampdiff_second_time_separator - timestampdiff_hour_offset] != ':') {
        return false;
    }
    if (!parse_digits(value, timestampdiff_two_digit_count, &time.hour) ||
        !parse_digits(
            value + timestampdiff_minute_offset - timestampdiff_hour_offset,
            timestampdiff_two_digit_count,
            &time.minute
        ) ||
        !parse_digits(
            value + timestampdiff_second_offset - timestampdiff_hour_offset,
            timestampdiff_two_digit_count,
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
        value = (value * timestampdiff_digit_radix) + (text[index] - '0');
    }
    *out_value = value;
    return true;
}

static bool date_is_valid(const struct timestampdiff_date_parts *date) {
    if (date == NULL || date->month < 1 || date->month > timestampdiff_month_max || date->day < 1 ||
        date->day > timestampdiff_day_max) {
        return false;
    }
    return date->day <= days_in_month(date->year, date->month);
}

static bool date_is_full_zero(const struct timestampdiff_date_parts *date) {
    if (date == NULL) {
        return false;
    }
    return (date->year == 0 && date->month == 0 && date->day == 0) != 0;
}

static bool date_is_partial_zero(const struct timestampdiff_date_parts *date) {
    if (date == NULL || date_is_full_zero(date)) {
        return false;
    }
    return (date->month == 0 || date->day == 0) != 0;
}

static bool time_is_valid(const struct timestampdiff_time_parts *time) {
    if (time == NULL) {
        return false;
    }
    return (time->hour >= 0 && time->hour <= timestampdiff_hour_max && time->minute >= 0 &&
            time->minute <= timestampdiff_minute_second_max && time->second >= 0 &&
            time->second <= timestampdiff_minute_second_max) != 0;
}

static int days_in_month(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == timestampdiff_february && is_leap_year(year)) {
        return timestampdiff_leap_day;
    }
    if (month < 1 || month > timestampdiff_month_max) {
        return 0;
    }
    return days[month - 1];
}

static bool is_leap_year(int year) {
    if (year == 0) {
        return false;
    }
    return ((year % timestampdiff_leap_quadrennial_year_cycle == 0 &&
             year % timestampdiff_leap_century_year_cycle != 0) ||
            year % timestampdiff_leap_quadricentennial_year_cycle == 0) != 0;
}

static int day_of_year(const struct timestampdiff_date_parts *date) {
    int result = 0;

    if (date == NULL) {
        return 0;
    }
    for (int month = 1; month < date->month; ++month) {
        result += days_in_month(date->year, month);
    }
    return result + date->day;
}

static int64_t day_number(const struct timestampdiff_date_parts *date) {
    int64_t year = date == NULL ? 0 : date->year;
    int64_t years_before = year <= 0 ? 0 : year - 1;
    int64_t leap_days_before_year = (years_before / timestampdiff_leap_quadrennial_year_cycle) -
                                    (years_before / timestampdiff_leap_century_year_cycle) +
                                    (years_before / timestampdiff_leap_quadricentennial_year_cycle);

    if (year > 0) {
        return (year * timestampdiff_days_per_common_year) + leap_days_before_year +
               day_of_year(date);
    }
    return day_of_year(date);
}

static int seconds_of_day(const struct timestampdiff_time_parts *time) {
    if (time == NULL) {
        return 0;
    }
    return (time->hour * timestampdiff_seconds_per_hour) +
           (time->minute * timestampdiff_seconds_per_minute) + time->second;
}

static int compare_day_time(
    const struct timestampdiff_datetime_parts *left,
    const struct timestampdiff_datetime_parts *right
) {
    int left_second = 0;
    int right_second = 0;

    if (left == NULL || right == NULL) {
        return 0;
    }
    if (left->date.day < right->date.day) {
        return -1;
    }
    if (left->date.day > right->date.day) {
        return 1;
    }
    left_second = seconds_of_day(&left->time);
    right_second = seconds_of_day(&right->time);
    if (left_second < right_second) {
        return -1;
    }
    if (left_second > right_second) {
        return 1;
    }
    return 0;
}

static int64_t whole_month_difference(
    const struct timestampdiff_datetime_parts *left,
    const struct timestampdiff_datetime_parts *right
) {
    int64_t months = 0;
    int day_time_comparison = 0;

    if (left == NULL || right == NULL) {
        return 0;
    }
    months = ((int64_t)right->date.year - (int64_t)left->date.year) * timestampdiff_month_max;
    months += (int64_t)right->date.month - (int64_t)left->date.month;
    day_time_comparison = -compare_day_time(left, right);
    if (months > 0 && day_time_comparison < 0) {
        --months;
    } else if (months < 0 && day_time_comparison > 0) {
        ++months;
    }
    return months;
}

static int64_t elapsed_second_difference(
    const struct timestampdiff_datetime_parts *left,
    const struct timestampdiff_datetime_parts *right
) {
    int64_t day_difference = 0;

    if (left == NULL || right == NULL) {
        return 0;
    }
    day_difference = day_number(&right->date) - day_number(&left->date);
    return (day_difference * timestampdiff_seconds_per_day) + seconds_of_day(&right->time) -
           seconds_of_day(&left->time);
}

static int64_t timestampdiff_result(
    enum mylite_timestampdiff_unit unit,
    const struct timestampdiff_datetime_parts *left,
    const struct timestampdiff_datetime_parts *right
) {
    int64_t months = 0;
    int64_t seconds = 0;

    switch (unit) {
    case MYLITE_TIMESTAMPDIFF_UNIT_YEAR:
        months = whole_month_difference(left, right);
        return months / timestampdiff_month_max;
    case MYLITE_TIMESTAMPDIFF_UNIT_QUARTER:
        months = whole_month_difference(left, right);
        return months / 3;
    case MYLITE_TIMESTAMPDIFF_UNIT_MONTH:
        return whole_month_difference(left, right);
    case MYLITE_TIMESTAMPDIFF_UNIT_WEEK:
        seconds = elapsed_second_difference(left, right);
        return seconds / timestampdiff_seconds_per_week;
    case MYLITE_TIMESTAMPDIFF_UNIT_DAY:
        seconds = elapsed_second_difference(left, right);
        return seconds / timestampdiff_seconds_per_day;
    case MYLITE_TIMESTAMPDIFF_UNIT_HOUR:
        seconds = elapsed_second_difference(left, right);
        return seconds / timestampdiff_seconds_per_hour;
    case MYLITE_TIMESTAMPDIFF_UNIT_MINUTE:
        seconds = elapsed_second_difference(left, right);
        return seconds / timestampdiff_seconds_per_minute;
    case MYLITE_TIMESTAMPDIFF_UNIT_SECOND:
        return elapsed_second_difference(left, right);
    case MYLITE_TIMESTAMPDIFF_UNIT_MICROSECOND:
        return 0;
    }
    return 0;
}

static int format_timestampdiff_result(struct mylite_db *database, int64_t value, char **out_text) {
    char buffer[timestampdiff_result_capacity];
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
