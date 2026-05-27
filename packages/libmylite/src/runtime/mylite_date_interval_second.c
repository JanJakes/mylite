#include "mylite_date_interval_second.h"

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
    mysql_warning_datetime_interval_overflow = 1441,
    mysql_warning_incorrect_datetime_value = 1292,
    date_interval_second_date_text_length = 10,
    date_interval_second_datetime_text_length = 19,
    date_interval_second_first_date_separator = 4,
    date_interval_second_month_offset = 5,
    date_interval_second_second_date_separator = 7,
    date_interval_second_day_offset = 8,
    date_interval_second_time_separator = 10,
    date_interval_second_hour_offset = 11,
    date_interval_second_first_time_separator = 13,
    date_interval_second_minute_offset = 14,
    date_interval_second_second_time_separator = 16,
    date_interval_second_second_offset = 17,
    date_interval_second_two_digit_count = 2,
    date_interval_second_four_digit_count = 4,
    date_interval_second_month_max = 12,
    date_interval_second_february = 2,
    date_interval_second_leap_day = 29,
    date_interval_second_hour_max = 23,
    date_interval_second_minute_second_max = 59,
    date_interval_second_year_minimum = 1000,
    date_interval_second_year_maximum = 9999,
    date_interval_second_digit_radix = 10,
    date_interval_second_hours_per_day = 24,
    date_interval_second_seconds_per_hour = 3600,
    date_interval_second_seconds_per_minute = 60,
    date_interval_second_days_per_week = 7,
    date_interval_second_months_per_quarter = 3,
    date_interval_second_sqlite_temporal_argument = 0,
    date_interval_second_sqlite_input_kind_argument = 1,
    date_interval_second_sqlite_interval_argument = 2,
    date_interval_second_sqlite_unit_argument = 3,
    date_interval_second_sqlite_operation_argument = 4,
    date_interval_second_sqlite_argument_count = 5,
    date_interval_second_march_year_shift_month = 2,
    date_interval_second_era_year_offset = 399,
    date_interval_second_years_per_era = 400,
    date_interval_second_month_shift = 3,
    date_interval_second_march_based_months_after_february = 9,
    date_interval_second_month_scale = 153,
    date_interval_second_month_bias = 2,
    date_interval_second_month_divisor = 5,
    date_interval_second_days_per_non_leap_year = 365,
    date_interval_second_days_per_era = 146097,
    date_interval_second_unix_epoch_day_offset = 719468,
    date_interval_second_leap_quadrennial = 4,
    date_interval_second_leap_century = 100,
    date_interval_second_days_per_era_offset = 146096,
    date_interval_second_leap_cycle_four_year_days = 1460,
    date_interval_second_leap_cycle_century_days = 36524,
    date_interval_second_march_based_month_switch = 10,
    date_interval_second_result_capacity = 20,
};

struct date_interval_second_datetime {
    int64_t year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
};

struct date_interval_second_day_second {
    int64_t days;
    int64_t day_seconds;
};

struct date_interval_second_source {
    const char *value;
    size_t value_length;
    enum mylite_date_interval_second_input_kind input_kind;
    bool is_null;
};

struct date_interval_second_sqlite_arguments {
    sqlite3_value *temporal;
    sqlite3_value *interval;
    sqlite3_value *unit;
    sqlite3_value *operation;
};

static void date_interval_second_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static void date_interval_update_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static int date_interval_second_sqlite_input_kind(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_date_interval_second_input_kind *out_kind
);
static int date_interval_second_sqlite_value(
    sqlite3_context *context,
    sqlite3_value *value,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int date_interval_second_sqlite_result(
    sqlite3_context *context,
    enum mylite_date_interval_second_input_kind input_kind,
    const struct date_interval_second_sqlite_arguments *arguments
);
static int date_interval_update_sqlite_result(
    sqlite3_context *context,
    enum mylite_date_interval_second_input_kind input_kind,
    const struct date_interval_second_sqlite_arguments *arguments
);

static int date_interval_second_result(
    struct mylite_db *database,
    const struct date_interval_second_source *source,
    int64_t interval_value,
    bool interval_is_null,
    enum mylite_date_interval_unit unit,
    bool subtract,
    const char *overflow_message,
    char **out_text,
    bool *out_is_null
);
static int date_interval_update_result(
    struct mylite_db *database,
    const struct date_interval_second_source *source,
    int64_t interval_value,
    bool interval_is_null,
    enum mylite_date_interval_unit unit,
    bool subtract,
    char **out_text,
    bool *out_is_null
);
static bool parse_date_interval_second_value(
    const struct date_interval_second_source *source,
    struct date_interval_second_datetime *out_datetime,
    bool *out_has_time
);
static bool parse_date_interval_second_date(
    const char *value,
    size_t value_length,
    struct date_interval_second_datetime *out_datetime
);
static bool parse_date_interval_second_datetime(
    const char *value,
    size_t value_length,
    struct date_interval_second_datetime *out_datetime
);
static bool parse_date_interval_second_time(
    const char *value,
    size_t value_length,
    int *out_hour,
    int *out_minute,
    int *out_second
);
static bool parse_date_interval_second_digits(const char *text, size_t length, int *out_value);
static bool date_interval_second_date_is_valid(
    const struct date_interval_second_datetime *datetime
);
static bool date_interval_second_time_is_valid(
    const struct date_interval_second_datetime *datetime
);
static int date_interval_second_days_in_month(int year, int month);
static bool date_interval_second_is_leap_year(int year);
static bool date_interval_second_add(
    const struct date_interval_second_datetime *input,
    int64_t interval_seconds,
    struct date_interval_second_datetime *out_datetime
);
static bool date_interval_unit_add(
    const struct date_interval_second_datetime *input,
    int64_t interval_value,
    enum mylite_date_interval_unit unit,
    struct date_interval_second_datetime *out_datetime
);
static bool date_interval_add_calendar_months(
    const struct date_interval_second_datetime *input,
    int64_t interval_months,
    struct date_interval_second_datetime *out_datetime
);
static bool date_interval_second_checked_add(int64_t left, int64_t right, int64_t *out_value);
static bool date_interval_second_checked_multiply(int64_t left, int64_t right, int64_t *out_value);
static int64_t date_interval_second_seconds_per_day(void);
static int64_t date_interval_second_days_from_datetime(
    const struct date_interval_second_datetime *datetime
);
static void date_interval_second_datetime_from_days(
    int64_t days,
    struct date_interval_second_datetime *out_datetime
);
static struct date_interval_second_day_second date_interval_second_floor_divmod(
    int64_t total_seconds
);
static int format_date_interval_second_result(
    struct mylite_db *database,
    const struct date_interval_second_datetime *datetime,
    bool result_has_time,
    char **out_text
);
static int append_date_interval_second_incorrect_datetime_warning(
    struct mylite_db *database,
    const char *value,
    size_t value_length
);
static int append_date_interval_second_overflow_warning(
    struct mylite_db *database,
    const char *message
);
static int set_date_interval_second_incorrect_datetime_error(
    struct mylite_db *database,
    const char *value,
    size_t value_length
);
static void set_date_interval_second_overflow_error(
    struct mylite_db *database,
    const char *message
);
static bool date_interval_ascii_equals_case_insensitive(
    const char *left,
    size_t left_length,
    const char *right
);

const char *mylite_date_interval_second_input_kind_name(
    enum mylite_date_interval_second_input_kind kind
) {
    switch (kind) {
    case MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING:
        return "string";
    case MYLITE_DATE_INTERVAL_SECOND_INPUT_DATE:
        return "date";
    case MYLITE_DATE_INTERVAL_SECOND_INPUT_DATETIME:
        return "datetime";
    case MYLITE_DATE_INTERVAL_SECOND_INPUT_TIMESTAMP:
        return "timestamp";
    }
    return NULL;
}

bool mylite_date_interval_second_input_kind_from_name(
    const char *name,
    size_t name_length,
    enum mylite_date_interval_second_input_kind *out_kind
) {
    static const struct {
        const char *name;
        enum mylite_date_interval_second_input_kind kind;
    } names[] = {
        {"string", MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING},
        {"date", MYLITE_DATE_INTERVAL_SECOND_INPUT_DATE},
        {"datetime", MYLITE_DATE_INTERVAL_SECOND_INPUT_DATETIME},
        {"timestamp", MYLITE_DATE_INTERVAL_SECOND_INPUT_TIMESTAMP},
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

const char *mylite_date_interval_unit_name(enum mylite_date_interval_unit unit) {
    switch (unit) {
    case MYLITE_DATE_INTERVAL_UNIT_YEAR:
        return "YEAR";
    case MYLITE_DATE_INTERVAL_UNIT_QUARTER:
        return "QUARTER";
    case MYLITE_DATE_INTERVAL_UNIT_MONTH:
        return "MONTH";
    case MYLITE_DATE_INTERVAL_UNIT_WEEK:
        return "WEEK";
    case MYLITE_DATE_INTERVAL_UNIT_DAY:
        return "DAY";
    case MYLITE_DATE_INTERVAL_UNIT_HOUR:
        return "HOUR";
    case MYLITE_DATE_INTERVAL_UNIT_MINUTE:
        return "MINUTE";
    case MYLITE_DATE_INTERVAL_UNIT_SECOND:
        return "SECOND";
    case MYLITE_DATE_INTERVAL_UNIT_MICROSECOND:
        return "MICROSECOND";
    }
    return NULL;
}

bool mylite_date_interval_unit_from_name(
    const char *name,
    size_t name_length,
    enum mylite_date_interval_unit *out_unit
) {
    static const struct {
        const char *name;
        enum mylite_date_interval_unit unit;
    } names[] = {
        {"YEAR", MYLITE_DATE_INTERVAL_UNIT_YEAR},
        {"QUARTER", MYLITE_DATE_INTERVAL_UNIT_QUARTER},
        {"MONTH", MYLITE_DATE_INTERVAL_UNIT_MONTH},
        {"WEEK", MYLITE_DATE_INTERVAL_UNIT_WEEK},
        {"DAY", MYLITE_DATE_INTERVAL_UNIT_DAY},
        {"HOUR", MYLITE_DATE_INTERVAL_UNIT_HOUR},
        {"MINUTE", MYLITE_DATE_INTERVAL_UNIT_MINUTE},
        {"SECOND", MYLITE_DATE_INTERVAL_UNIT_SECOND},
        {"MICROSECOND", MYLITE_DATE_INTERVAL_UNIT_MICROSECOND},
    };

    if (name == NULL || out_unit == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        if (date_interval_ascii_equals_case_insensitive(name, name_length, names[index].name)) {
            *out_unit = names[index].unit;
            return true;
        }
    }
    return false;
}

static bool date_interval_ascii_equals_case_insensitive(
    const char *left,
    size_t left_length,
    const char *right
) {
    size_t right_length = right == NULL ? 0U : strlen(right);

    if (left == NULL || right == NULL || left_length != right_length) {
        return false;
    }
    for (size_t index = 0U; index < left_length; ++index) {
        unsigned char left_byte = (unsigned char)left[index];
        unsigned char right_byte = (unsigned char)right[index];

        if (left_byte >= 'a' && left_byte <= 'z') {
            left_byte = (unsigned char)(left_byte - ('a' - 'A'));
        }
        if (right_byte >= 'a' && right_byte <= 'z') {
            right_byte = (unsigned char)(right_byte - ('a' - 'A'));
        }
        if (left_byte != right_byte) {
            return false;
        }
    }
    return true;
}

bool mylite_date_interval_unit_has_time_part(enum mylite_date_interval_unit unit) {
    switch (unit) {
    case MYLITE_DATE_INTERVAL_UNIT_HOUR:
    case MYLITE_DATE_INTERVAL_UNIT_MINUTE:
    case MYLITE_DATE_INTERVAL_UNIT_SECOND:
    case MYLITE_DATE_INTERVAL_UNIT_MICROSECOND:
        return true;
    case MYLITE_DATE_INTERVAL_UNIT_YEAR:
    case MYLITE_DATE_INTERVAL_UNIT_QUARTER:
    case MYLITE_DATE_INTERVAL_UNIT_MONTH:
    case MYLITE_DATE_INTERVAL_UNIT_WEEK:
    case MYLITE_DATE_INTERVAL_UNIT_DAY:
        return false;
    }
    return true;
}

int mylite_date_interval_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    enum mylite_date_interval_second_input_kind input_kind,
    bool value_is_null,
    int64_t interval_value,
    bool interval_is_null,
    enum mylite_date_interval_unit unit,
    bool subtract,
    char **out_text,
    bool *out_is_null
) {
    return mylite_date_interval_value_with_overflow_message(
        database,
        value,
        value_length,
        input_kind,
        value_is_null,
        interval_value,
        interval_is_null,
        unit,
        subtract,
        "Datetime function: datetime field overflow",
        out_text,
        out_is_null
    );
}

int mylite_date_interval_value_with_overflow_message(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    enum mylite_date_interval_second_input_kind input_kind,
    bool value_is_null,
    int64_t interval_value,
    bool interval_is_null,
    enum mylite_date_interval_unit unit,
    bool subtract,
    const char *overflow_message,
    char **out_text,
    bool *out_is_null
) {
    const struct date_interval_second_source source = {
        .value = value,
        .value_length = value_length,
        .input_kind = input_kind,
        .is_null = value_is_null,
    };

    if (out_text == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_is_null = false;
    return date_interval_second_result(
        database,
        &source,
        interval_value,
        interval_is_null,
        unit,
        subtract,
        overflow_message,
        out_text,
        out_is_null
    );
}

int mylite_date_interval_second_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    enum mylite_date_interval_second_input_kind input_kind,
    bool value_is_null,
    int64_t interval_seconds,
    bool interval_is_null,
    bool subtract,
    char **out_text,
    bool *out_is_null
) {
    return mylite_date_interval_value_with_overflow_message(
        database,
        value,
        value_length,
        input_kind,
        value_is_null,
        interval_seconds,
        interval_is_null,
        MYLITE_DATE_INTERVAL_UNIT_SECOND,
        subtract,
        "Datetime function: datetime field overflow",
        out_text,
        out_is_null
    );
}

int mylite_date_interval_second_value_with_overflow_message(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    enum mylite_date_interval_second_input_kind input_kind,
    bool value_is_null,
    int64_t interval_seconds,
    bool interval_is_null,
    bool subtract,
    const char *overflow_message,
    char **out_text,
    bool *out_is_null
) {
    return mylite_date_interval_value_with_overflow_message(
        database,
        value,
        value_length,
        input_kind,
        value_is_null,
        interval_seconds,
        interval_is_null,
        MYLITE_DATE_INTERVAL_UNIT_SECOND,
        subtract,
        overflow_message,
        out_text,
        out_is_null
    );
}

int mylite_sqlite_register_date_interval_second_function(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_date_interval_second",
            .argument_count = date_interval_second_sqlite_argument_count,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = date_interval_second_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_date_interval_update",
            .argument_count = date_interval_second_sqlite_argument_count,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = date_interval_update_sqlite_callback,
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

static void date_interval_second_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    enum mylite_date_interval_second_input_kind input_kind =
        MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING;
    struct date_interval_second_sqlite_arguments arguments = {0};

    if (context == NULL || argc != date_interval_second_sqlite_argument_count || argv == NULL ||
        argv[date_interval_second_sqlite_temporal_argument] == NULL ||
        argv[date_interval_second_sqlite_input_kind_argument] == NULL ||
        argv[date_interval_second_sqlite_interval_argument] == NULL ||
        argv[date_interval_second_sqlite_unit_argument] == NULL ||
        argv[date_interval_second_sqlite_operation_argument] == NULL) {
        sqlite3_result_error(context, "invalid MyLite DATE interval callback", -1);
        return;
    }
    arguments = (struct date_interval_second_sqlite_arguments){
        .temporal = argv[date_interval_second_sqlite_temporal_argument],
        .interval = argv[date_interval_second_sqlite_interval_argument],
        .unit = argv[date_interval_second_sqlite_unit_argument],
        .operation = argv[date_interval_second_sqlite_operation_argument],
    };
    if (sqlite3_value_type(argv[date_interval_second_sqlite_input_kind_argument]) == SQLITE_NULL ||
        sqlite3_value_type(arguments.unit) == SQLITE_NULL ||
        sqlite3_value_type(arguments.operation) == SQLITE_NULL) {
        sqlite3_result_error(context, "invalid MyLite DATE interval discriminator", -1);
        return;
    }
    if (date_interval_second_sqlite_input_kind(
            context,
            argv[date_interval_second_sqlite_input_kind_argument],
            &input_kind
        ) != MYLITE_OK) {
        return;
    }
    if (date_interval_second_sqlite_result(context, input_kind, &arguments) != MYLITE_OK) {
        return;
    }
}

static void date_interval_update_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    enum mylite_date_interval_second_input_kind input_kind =
        MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING;
    struct date_interval_second_sqlite_arguments arguments = {0};

    if (context == NULL || argc != date_interval_second_sqlite_argument_count || argv == NULL ||
        argv[date_interval_second_sqlite_temporal_argument] == NULL ||
        argv[date_interval_second_sqlite_input_kind_argument] == NULL ||
        argv[date_interval_second_sqlite_interval_argument] == NULL ||
        argv[date_interval_second_sqlite_unit_argument] == NULL ||
        argv[date_interval_second_sqlite_operation_argument] == NULL) {
        sqlite3_result_error(context, "invalid MyLite DATE interval update callback", -1);
        return;
    }
    arguments = (struct date_interval_second_sqlite_arguments){
        .temporal = argv[date_interval_second_sqlite_temporal_argument],
        .interval = argv[date_interval_second_sqlite_interval_argument],
        .unit = argv[date_interval_second_sqlite_unit_argument],
        .operation = argv[date_interval_second_sqlite_operation_argument],
    };
    if (sqlite3_value_type(argv[date_interval_second_sqlite_input_kind_argument]) == SQLITE_NULL ||
        sqlite3_value_type(arguments.unit) == SQLITE_NULL ||
        sqlite3_value_type(arguments.operation) == SQLITE_NULL) {
        sqlite3_result_error(context, "invalid MyLite DATE interval update discriminator", -1);
        return;
    }
    if (date_interval_second_sqlite_input_kind(
            context,
            argv[date_interval_second_sqlite_input_kind_argument],
            &input_kind
        ) != MYLITE_OK) {
        return;
    }
    if (date_interval_update_sqlite_result(context, input_kind, &arguments) != MYLITE_OK) {
        return;
    }
}

static int date_interval_second_sqlite_input_kind(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_date_interval_second_input_kind *out_kind
) {
    const unsigned char *kind_text = sqlite3_value_text(value);
    int kind_length = sqlite3_value_bytes(value);

    if (kind_text == NULL || kind_length < 0 || out_kind == NULL) {
        sqlite3_result_error_nomem(context);
        return MYLITE_NOMEM;
    }
    if (!mylite_date_interval_second_input_kind_from_name(
            (const char *)kind_text,
            (size_t)kind_length,
            out_kind
        )) {
        sqlite3_result_error(context, "invalid MyLite DATE interval input kind", -1);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int date_interval_second_sqlite_result(
    sqlite3_context *context,
    enum mylite_date_interval_second_input_kind input_kind,
    const struct date_interval_second_sqlite_arguments *arguments
) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    enum mylite_date_interval_unit unit = MYLITE_DATE_INTERVAL_UNIT_SECOND;
    const char *temporal_text = NULL;
    size_t temporal_text_length = 0U;
    bool temporal_is_null = false;
    int64_t interval_seconds = 0;
    bool interval_is_null = false;
    bool result_is_null = false;
    bool subtract = false;
    char *result = NULL;
    int rc = MYLITE_OK;

    if (arguments == NULL) {
        sqlite3_result_error(context, "invalid MyLite DATE interval callback", -1);
        return MYLITE_ERROR;
    }
    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite DATE interval owner", -1);
        return MYLITE_ERROR;
    }
    {
        const unsigned char *unit_text = sqlite3_value_text(arguments->unit);
        int unit_length = sqlite3_value_bytes(arguments->unit);

        if (unit_text == NULL || unit_length < 0) {
            sqlite3_result_error_nomem(context);
            return MYLITE_NOMEM;
        }
        if (!mylite_date_interval_unit_from_name(
                (const char *)unit_text,
                (size_t)unit_length,
                &unit
            )) {
            sqlite3_result_error(context, "invalid MyLite DATE interval unit", -1);
            return MYLITE_ERROR;
        }
    }
    rc = date_interval_second_sqlite_value(
        context,
        arguments->temporal,
        &temporal_text,
        &temporal_text_length,
        &temporal_is_null
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (sqlite3_value_type(arguments->interval) == SQLITE_NULL) {
        interval_is_null = true;
    } else {
        interval_seconds = (int64_t)sqlite3_value_int64(arguments->interval);
    }
    subtract = sqlite3_value_int64(arguments->operation) != 0;

    rc = mylite_date_interval_value(
        database,
        temporal_text,
        temporal_text_length,
        input_kind,
        temporal_is_null,
        interval_seconds,
        interval_is_null,
        unit,
        subtract,
        &result,
        &result_is_null
    );
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
        } else {
            sqlite3_result_error(context, "MyLite DATE interval failed", -1);
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

static int date_interval_update_sqlite_result(
    sqlite3_context *context,
    enum mylite_date_interval_second_input_kind input_kind,
    const struct date_interval_second_sqlite_arguments *arguments
) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    enum mylite_date_interval_unit unit = MYLITE_DATE_INTERVAL_UNIT_SECOND;
    const char *temporal_text = NULL;
    size_t temporal_text_length = 0U;
    bool temporal_is_null = false;
    int64_t interval_seconds = 0;
    bool interval_is_null = false;
    bool result_is_null = false;
    bool subtract = false;
    char *result = NULL;
    int rc = MYLITE_OK;

    if (arguments == NULL) {
        sqlite3_result_error(context, "invalid MyLite DATE interval update callback", -1);
        return MYLITE_ERROR;
    }
    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite DATE interval update owner", -1);
        return MYLITE_ERROR;
    }
    {
        const unsigned char *unit_text = sqlite3_value_text(arguments->unit);
        int unit_length = sqlite3_value_bytes(arguments->unit);

        if (unit_text == NULL || unit_length < 0) {
            sqlite3_result_error_nomem(context);
            return MYLITE_NOMEM;
        }
        if (!mylite_date_interval_unit_from_name(
                (const char *)unit_text,
                (size_t)unit_length,
                &unit
            )) {
            sqlite3_result_error(context, "invalid MyLite DATE interval update unit", -1);
            return MYLITE_ERROR;
        }
    }
    rc = date_interval_second_sqlite_value(
        context,
        arguments->temporal,
        &temporal_text,
        &temporal_text_length,
        &temporal_is_null
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (sqlite3_value_type(arguments->interval) == SQLITE_NULL) {
        interval_is_null = true;
    } else {
        interval_seconds = (int64_t)sqlite3_value_int64(arguments->interval);
    }
    subtract = sqlite3_value_int64(arguments->operation) != 0;

    rc = date_interval_update_result(
        database,
        &(struct date_interval_second_source){
            .value = temporal_text,
            .value_length = temporal_text_length,
            .input_kind = input_kind,
            .is_null = temporal_is_null,
        },
        interval_seconds,
        interval_is_null,
        unit,
        subtract,
        &result,
        &result_is_null
    );
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
        } else {
            sqlite3_result_error(
                context,
                mylite_diagnostics_errmsg(mylite_connection_diagnostics(database)),
                -1
            );
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

static int date_interval_second_sqlite_value(
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

static int date_interval_second_result(
    struct mylite_db *database,
    const struct date_interval_second_source *source,
    int64_t interval_value,
    bool interval_is_null,
    enum mylite_date_interval_unit unit,
    bool subtract,
    const char *overflow_message,
    char **out_text,
    bool *out_is_null
) {
    struct date_interval_second_datetime input = {0};
    struct date_interval_second_datetime output = {0};
    bool input_has_time = false;
    bool result_has_time = false;
    int rc = MYLITE_OK;

    if (out_text == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_is_null = false;
    if (source == NULL || source->is_null || interval_is_null) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (!parse_date_interval_second_value(source, &input, &input_has_time)) {
        rc = append_date_interval_second_incorrect_datetime_warning(
            database,
            source->value,
            source->value_length
        );
        if (rc == MYLITE_OK) {
            *out_is_null = true;
        }
        return rc;
    }
    if (subtract) {
        if (interval_value == INT64_MIN) {
            rc = append_date_interval_second_overflow_warning(database, overflow_message);
            if (rc == MYLITE_OK) {
                *out_is_null = true;
            }
            return rc;
        }
        interval_value = -interval_value;
    }
    if (!date_interval_unit_add(&input, interval_value, unit, &output)) {
        rc = append_date_interval_second_overflow_warning(database, overflow_message);
        if (rc == MYLITE_OK) {
            *out_is_null = true;
        }
        return rc;
    }
    result_has_time = (input_has_time || mylite_date_interval_unit_has_time_part(unit)) != 0;
    return format_date_interval_second_result(database, &output, result_has_time, out_text);
}

static int date_interval_update_result(
    struct mylite_db *database,
    const struct date_interval_second_source *source,
    int64_t interval_value,
    bool interval_is_null,
    enum mylite_date_interval_unit unit,
    bool subtract,
    char **out_text,
    bool *out_is_null
) {
    struct date_interval_second_datetime input = {0};
    struct date_interval_second_datetime output = {0};
    bool input_has_time = false;
    bool result_has_time = false;

    if (out_text == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_is_null = false;
    if (source == NULL || source->is_null || interval_is_null) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (!parse_date_interval_second_value(source, &input, &input_has_time)) {
        return set_date_interval_second_incorrect_datetime_error(
            database,
            source->value,
            source->value_length
        );
    }
    if (subtract) {
        if (interval_value == INT64_MIN) {
            set_date_interval_second_overflow_error(
                database,
                "Datetime function: datetime field overflow"
            );
            return MYLITE_ERROR;
        }
        interval_value = -interval_value;
    }
    if (!date_interval_unit_add(&input, interval_value, unit, &output)) {
        set_date_interval_second_overflow_error(
            database,
            "Datetime function: datetime field overflow"
        );
        return MYLITE_ERROR;
    }
    result_has_time = (input_has_time || mylite_date_interval_unit_has_time_part(unit)) != 0;
    return format_date_interval_second_result(database, &output, result_has_time, out_text);
}

static bool parse_date_interval_second_value(
    const struct date_interval_second_source *source,
    struct date_interval_second_datetime *out_datetime,
    bool *out_has_time
) {
    if (source == NULL || out_datetime == NULL || out_has_time == NULL || source->value == NULL) {
        return false;
    }
    *out_has_time = false;
    switch (source->input_kind) {
    case MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING:
        if (parse_date_interval_second_date(source->value, source->value_length, out_datetime)) {
            return true;
        }
        *out_has_time = true;
        return parse_date_interval_second_datetime(
            source->value,
            source->value_length,
            out_datetime
        );
    case MYLITE_DATE_INTERVAL_SECOND_INPUT_DATE:
        return parse_date_interval_second_date(source->value, source->value_length, out_datetime);
    case MYLITE_DATE_INTERVAL_SECOND_INPUT_DATETIME:
    case MYLITE_DATE_INTERVAL_SECOND_INPUT_TIMESTAMP:
        *out_has_time = true;
        return parse_date_interval_second_datetime(
            source->value,
            source->value_length,
            out_datetime
        );
    }
    return false;
}

static bool parse_date_interval_second_date(
    const char *value,
    size_t value_length,
    struct date_interval_second_datetime *out_datetime
) {
    int year = 0;
    int month = 0;
    int day = 0;

    if (value == NULL || out_datetime == NULL ||
        value_length != date_interval_second_date_text_length ||
        value[date_interval_second_first_date_separator] != '-' ||
        value[date_interval_second_second_date_separator] != '-') {
        return false;
    }
    if (!parse_date_interval_second_digits(value, date_interval_second_four_digit_count, &year) ||
        !parse_date_interval_second_digits(
            value + date_interval_second_month_offset,
            date_interval_second_two_digit_count,
            &month
        ) ||
        !parse_date_interval_second_digits(
            value + date_interval_second_day_offset,
            date_interval_second_two_digit_count,
            &day
        )) {
        return false;
    }
    *out_datetime = (struct date_interval_second_datetime){
        .year = year,
        .month = month,
        .day = day,
        .hour = 0,
        .minute = 0,
        .second = 0,
    };
    return date_interval_second_date_is_valid(out_datetime);
}

static bool parse_date_interval_second_datetime(
    const char *value,
    size_t value_length,
    struct date_interval_second_datetime *out_datetime
) {
    struct date_interval_second_datetime datetime = {0};

    if (value == NULL || out_datetime == NULL ||
        value_length != date_interval_second_datetime_text_length ||
        value[date_interval_second_time_separator] != ' ') {
        return false;
    }
    if (!parse_date_interval_second_date(value, date_interval_second_date_text_length, &datetime) ||
        !parse_date_interval_second_time(
            value + date_interval_second_hour_offset,
            date_interval_second_datetime_text_length - date_interval_second_hour_offset,
            &datetime.hour,
            &datetime.minute,
            &datetime.second
        )) {
        return false;
    }
    *out_datetime = datetime;
    return true;
}

static bool parse_date_interval_second_time(
    const char *value,
    size_t value_length,
    int *out_hour,
    int *out_minute,
    int *out_second
) {
    int hour = 0;
    int minute = 0;
    int second = 0;
    const size_t local_first_separator =
        date_interval_second_first_time_separator - date_interval_second_hour_offset;
    const size_t local_second_separator =
        date_interval_second_second_time_separator - date_interval_second_hour_offset;
    const size_t local_minute_offset =
        date_interval_second_minute_offset - date_interval_second_hour_offset;
    const size_t local_second_offset =
        date_interval_second_second_offset - date_interval_second_hour_offset;

    if (value == NULL || out_hour == NULL || out_minute == NULL || out_second == NULL ||
        value_length !=
            date_interval_second_datetime_text_length - date_interval_second_hour_offset ||
        value[local_first_separator] != ':' || value[local_second_separator] != ':') {
        return false;
    }
    if (!parse_date_interval_second_digits(value, date_interval_second_two_digit_count, &hour) ||
        !parse_date_interval_second_digits(
            value + local_minute_offset,
            date_interval_second_two_digit_count,
            &minute
        ) ||
        !parse_date_interval_second_digits(
            value + local_second_offset,
            date_interval_second_two_digit_count,
            &second
        )) {
        return false;
    }
    if (hour < 0 || hour > date_interval_second_hour_max || minute < 0 ||
        minute > date_interval_second_minute_second_max || second < 0 ||
        second > date_interval_second_minute_second_max) {
        return false;
    }
    *out_hour = hour;
    *out_minute = minute;
    *out_second = second;
    return true;
}

static bool parse_date_interval_second_digits(const char *text, size_t length, int *out_value) {
    int value = 0;

    if (text == NULL || out_value == NULL || length == 0U) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
        value = (value * date_interval_second_digit_radix) + (text[index] - '0');
    }
    *out_value = value;
    return true;
}

static bool date_interval_second_date_is_valid(
    const struct date_interval_second_datetime *datetime
) {
    if (datetime == NULL || datetime->year < date_interval_second_year_minimum ||
        datetime->year > date_interval_second_year_maximum || datetime->month < 1 ||
        datetime->month > date_interval_second_month_max || datetime->day < 1) {
        return false;
    }
    return datetime->day <=
           date_interval_second_days_in_month((int)datetime->year, datetime->month);
}

static bool date_interval_second_time_is_valid(
    const struct date_interval_second_datetime *datetime
) {
    if (datetime == NULL) {
        return false;
    }
    return (datetime->hour >= 0 && datetime->hour <= date_interval_second_hour_max &&
            datetime->minute >= 0 && datetime->minute <= date_interval_second_minute_second_max &&
            datetime->second >= 0 && datetime->second <= date_interval_second_minute_second_max) !=
           0;
}

static int date_interval_second_days_in_month(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == date_interval_second_february && date_interval_second_is_leap_year(year)) {
        return date_interval_second_leap_day;
    }
    if (month < 1 || month > date_interval_second_month_max) {
        return 0;
    }
    return days[month - 1];
}

static bool date_interval_second_is_leap_year(int year) {
    return ((year % date_interval_second_leap_quadrennial == 0 &&
             year % date_interval_second_leap_century != 0) ||
            year % date_interval_second_years_per_era == 0) != 0;
}

static bool date_interval_second_add(
    const struct date_interval_second_datetime *input,
    int64_t interval_seconds,
    struct date_interval_second_datetime *out_datetime
) {
    const int64_t seconds_per_day = date_interval_second_seconds_per_day();
    struct date_interval_second_day_second result = {0};
    int64_t days = 0;
    int64_t day_seconds = 0;
    int64_t base_seconds = 0;
    int64_t result_seconds = 0;

    if (input == NULL || out_datetime == NULL || !date_interval_second_time_is_valid(input)) {
        return false;
    }
    *out_datetime = (struct date_interval_second_datetime){0};
    days = date_interval_second_days_from_datetime(input);
    day_seconds = ((int64_t)input->hour * date_interval_second_seconds_per_hour) +
                  ((int64_t)input->minute * date_interval_second_seconds_per_minute) +
                  (int64_t)input->second;
    base_seconds = (days * seconds_per_day) + day_seconds;
    if (!date_interval_second_checked_add(base_seconds, interval_seconds, &result_seconds)) {
        return false;
    }

    result = date_interval_second_floor_divmod(result_seconds);
    date_interval_second_datetime_from_days(result.days, out_datetime);
    if (out_datetime->year < date_interval_second_year_minimum ||
        out_datetime->year > date_interval_second_year_maximum) {
        return false;
    }
    out_datetime->hour = (int)(result.day_seconds / date_interval_second_seconds_per_hour);
    result.day_seconds %= date_interval_second_seconds_per_hour;
    out_datetime->minute = (int)(result.day_seconds / date_interval_second_seconds_per_minute);
    out_datetime->second = (int)(result.day_seconds % date_interval_second_seconds_per_minute);
    return true;
}

static bool date_interval_unit_add(
    const struct date_interval_second_datetime *input,
    int64_t interval_value,
    enum mylite_date_interval_unit unit,
    struct date_interval_second_datetime *out_datetime
) {
    int64_t scaled_interval = interval_value;

    switch (unit) {
    case MYLITE_DATE_INTERVAL_UNIT_SECOND:
        return date_interval_second_add(input, interval_value, out_datetime);
    case MYLITE_DATE_INTERVAL_UNIT_MINUTE:
        if (!date_interval_second_checked_multiply(
                interval_value,
                date_interval_second_seconds_per_minute,
                &scaled_interval
            )) {
            return false;
        }
        return date_interval_second_add(input, scaled_interval, out_datetime);
    case MYLITE_DATE_INTERVAL_UNIT_HOUR:
        if (!date_interval_second_checked_multiply(
                interval_value,
                date_interval_second_seconds_per_hour,
                &scaled_interval
            )) {
            return false;
        }
        return date_interval_second_add(input, scaled_interval, out_datetime);
    case MYLITE_DATE_INTERVAL_UNIT_DAY:
        if (!date_interval_second_checked_multiply(
                interval_value,
                date_interval_second_seconds_per_day(),
                &scaled_interval
            )) {
            return false;
        }
        return date_interval_second_add(input, scaled_interval, out_datetime);
    case MYLITE_DATE_INTERVAL_UNIT_WEEK:
        if (!date_interval_second_checked_multiply(
                interval_value,
                (int64_t)date_interval_second_days_per_week *
                    date_interval_second_seconds_per_day(),
                &scaled_interval
            )) {
            return false;
        }
        return date_interval_second_add(input, scaled_interval, out_datetime);
    case MYLITE_DATE_INTERVAL_UNIT_MONTH:
        return date_interval_add_calendar_months(input, interval_value, out_datetime);
    case MYLITE_DATE_INTERVAL_UNIT_QUARTER:
        if (!date_interval_second_checked_multiply(
                interval_value,
                date_interval_second_months_per_quarter,
                &scaled_interval
            )) {
            return false;
        }
        return date_interval_add_calendar_months(input, scaled_interval, out_datetime);
    case MYLITE_DATE_INTERVAL_UNIT_YEAR:
        if (!date_interval_second_checked_multiply(
                interval_value,
                date_interval_second_month_max,
                &scaled_interval
            )) {
            return false;
        }
        return date_interval_add_calendar_months(input, scaled_interval, out_datetime);
    case MYLITE_DATE_INTERVAL_UNIT_MICROSECOND:
        return false;
    }
    return false;
}

static bool date_interval_add_calendar_months(
    const struct date_interval_second_datetime *input,
    int64_t interval_months,
    struct date_interval_second_datetime *out_datetime
) {
    int64_t base_months = 0;
    int64_t result_months = 0;
    int64_t year = 0;
    int64_t month_index = 0;
    int days_in_target_month = 0;

    if (input == NULL || out_datetime == NULL || !date_interval_second_time_is_valid(input)) {
        return false;
    }
    if (!date_interval_second_checked_multiply(
            input->year,
            date_interval_second_month_max,
            &base_months
        ) ||
        !date_interval_second_checked_add(base_months, (int64_t)input->month - 1, &base_months) ||
        !date_interval_second_checked_add(base_months, interval_months, &result_months)) {
        return false;
    }

    year = result_months / date_interval_second_month_max;
    month_index = result_months % date_interval_second_month_max;
    if (month_index < 0) {
        month_index += date_interval_second_month_max;
        --year;
    }
    if (year < date_interval_second_year_minimum || year > date_interval_second_year_maximum) {
        return false;
    }
    *out_datetime = *input;
    out_datetime->year = year;
    out_datetime->month = (int)month_index + 1;
    days_in_target_month =
        date_interval_second_days_in_month((int)out_datetime->year, out_datetime->month);
    if (out_datetime->day > days_in_target_month) {
        out_datetime->day = days_in_target_month;
    }
    return date_interval_second_date_is_valid(out_datetime);
}

static bool date_interval_second_checked_add(int64_t left, int64_t right, int64_t *out_value) {
    if (out_value == NULL) {
        return false;
    }
    if ((right > 0 && left > INT64_MAX - right) || (right < 0 && left < INT64_MIN - right)) {
        return false;
    }
    *out_value = left + right;
    return true;
}

static bool date_interval_second_checked_multiply(int64_t left, int64_t right, int64_t *out_value) {
    if (out_value == NULL) {
        return false;
    }
    if (left != 0 && (left > INT64_MAX / right || left < INT64_MIN / right)) {
        return false;
    }
    *out_value = left * right;
    return true;
}

static int64_t date_interval_second_seconds_per_day(void) {
    return (int64_t)date_interval_second_hours_per_day *
           (int64_t)date_interval_second_seconds_per_hour;
}

static int64_t date_interval_second_days_from_datetime(
    const struct date_interval_second_datetime *datetime
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
    year -= datetime->month <= date_interval_second_march_year_shift_month ? 1 : 0;
    era = (year >= 0 ? year : year - date_interval_second_era_year_offset) /
          date_interval_second_years_per_era;
    year_of_era = (uint32_t)(year - (era * date_interval_second_years_per_era));
    month_prime =
        datetime->month > date_interval_second_march_year_shift_month
            ? (uint32_t)(datetime->month - date_interval_second_month_shift)
            : (uint32_t)(datetime->month + date_interval_second_march_based_months_after_february);
    day_of_year =
        (((date_interval_second_month_scale * month_prime) + date_interval_second_month_bias) /
         date_interval_second_month_divisor) +
        (uint32_t)datetime->day - 1U;
    day_of_era = (year_of_era * date_interval_second_days_per_non_leap_year) +
                 (year_of_era / date_interval_second_leap_quadrennial) -
                 (year_of_era / date_interval_second_leap_century) + day_of_year;

    return (era * date_interval_second_days_per_era) + (int64_t)day_of_era -
           date_interval_second_unix_epoch_day_offset;
}

static void date_interval_second_datetime_from_days(
    int64_t days,
    struct date_interval_second_datetime *out_datetime
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
    days += date_interval_second_unix_epoch_day_offset;
    era = (days >= 0 ? days : days - date_interval_second_days_per_era_offset) /
          date_interval_second_days_per_era;
    day_of_era = (uint32_t)(days - (era * date_interval_second_days_per_era));
    year_of_era = (day_of_era - (day_of_era / date_interval_second_leap_cycle_four_year_days) +
                   (day_of_era / date_interval_second_leap_cycle_century_days) -
                   (day_of_era / date_interval_second_days_per_era_offset)) /
                  date_interval_second_days_per_non_leap_year;
    year = (int64_t)year_of_era + (era * date_interval_second_years_per_era);
    day_of_year = day_of_era - ((date_interval_second_days_per_non_leap_year * year_of_era) +
                                (year_of_era / date_interval_second_leap_quadrennial) -
                                (year_of_era / date_interval_second_leap_century));
    month_prime =
        ((date_interval_second_month_divisor * day_of_year) + date_interval_second_month_bias) /
        date_interval_second_month_scale;

    out_datetime->day = (int)(day_of_year -
                              (((date_interval_second_month_scale * month_prime) +
                                date_interval_second_month_bias) /
                               date_interval_second_month_divisor) +
                              1U);
    out_datetime->month =
        (int)(month_prime < date_interval_second_march_based_month_switch
                  ? month_prime + date_interval_second_month_shift
                  : month_prime - date_interval_second_march_based_months_after_february);
    out_datetime->year =
        year + (out_datetime->month <= date_interval_second_march_year_shift_month ? 1 : 0);
}

static struct date_interval_second_day_second date_interval_second_floor_divmod(
    int64_t total_seconds
) {
    const int64_t seconds_per_day = date_interval_second_seconds_per_day();
    struct date_interval_second_day_second result = {0};

    result.days = total_seconds / seconds_per_day;
    result.day_seconds = total_seconds % seconds_per_day;
    if (result.day_seconds < 0) {
        result.day_seconds += seconds_per_day;
        --result.days;
    }
    return result;
}

static int format_date_interval_second_result(
    struct mylite_db *database,
    const struct date_interval_second_datetime *datetime,
    bool result_has_time,
    char **out_text
) {
    char *text = NULL;
    int written = 0;

    if (datetime == NULL || out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    text = (char *)malloc(date_interval_second_result_capacity);
    if (text == NULL) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_NOMEM,
            "HY001",
            "out of memory"
        );
        return MYLITE_NOMEM;
    }
    if (result_has_time) {
        written = snprintf(
            text,
            date_interval_second_result_capacity,
            "%04" PRId64 "-%02d-%02d %02d:%02d:%02d",
            datetime->year,
            datetime->month,
            datetime->day,
            datetime->hour,
            datetime->minute,
            datetime->second
        );
    } else {
        written = snprintf(
            text,
            date_interval_second_result_capacity,
            "%04" PRId64 "-%02d-%02d",
            datetime->year,
            datetime->month,
            datetime->day
        );
    }
    if ((result_has_time && written != date_interval_second_datetime_text_length) ||
        (!result_has_time && written != date_interval_second_date_text_length)) {
        free(text);
        return append_date_interval_second_overflow_warning(
            database,
            "Datetime function: datetime field overflow"
        );
    }
    *out_text = text;
    return MYLITE_OK;
}

static int append_date_interval_second_incorrect_datetime_warning(
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

static int append_date_interval_second_overflow_warning(
    struct mylite_db *database,
    const char *message
) {
    int rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_datetime_interval_overflow,
        "HY000",
        message == NULL ? "Datetime function: datetime field overflow" : message
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

static int set_date_interval_second_incorrect_datetime_error(
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

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_warning_incorrect_datetime_value,
        "22007",
        message
    );
    return MYLITE_ERROR;
}

static void set_date_interval_second_overflow_error(
    struct mylite_db *database,
    const char *message
) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_warning_datetime_interval_overflow,
        "22008",
        message == NULL ? "Datetime function: datetime field overflow" : message
    );
}
