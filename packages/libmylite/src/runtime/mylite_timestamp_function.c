#include "mylite_timestamp_function.h"

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
    mysql_warning_incorrect_temporal_value = 1292,
    timestamp_time_seconds_per_minute = 60,
    timestamp_time_minutes_per_hour = 60,
    timestamp_time_hours_per_day = 24,
    timestamp_time_hour_max = 838,
    timestamp_time_minute_second_max = 59,
    timestamp_time_value_preview_length = 200,
    timestamp_time_digit_radix = 10,
};

static const int64_t timestamp_time_max_seconds =
    ((int64_t)timestamp_time_hour_max * timestamp_time_minutes_per_hour *
     timestamp_time_seconds_per_minute) +
    ((int64_t)timestamp_time_minute_second_max * timestamp_time_seconds_per_minute) +
    timestamp_time_minute_second_max;

static void timestamp_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static int timestamp_sqlite_input_kind(
    sqlite3_context *context,
    sqlite3_value *value,
    enum mylite_date_interval_second_input_kind *out_kind
);
static int timestamp_sqlite_text_value(
    sqlite3_context *context,
    sqlite3_value *value,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
);
static int timestamp_sqlite_result(sqlite3_context *context, char *result, bool is_null, int rc);
static int timestamp_time_argument(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    int64_t *out_seconds,
    bool *out_is_null
);
static bool parse_timestamp_time_text(
    const char *value,
    size_t value_length,
    int64_t *out_seconds,
    bool *out_clipped
);
static bool parse_timestamp_time_sign(
    const char *value,
    size_t value_length,
    size_t *inout_index,
    bool *out_negative
);
static bool parse_timestamp_time_day_prefix(
    const char *value,
    size_t value_length,
    size_t *inout_time_start,
    uint64_t *out_days
);
static bool parse_timestamp_time_components(
    const char *value,
    size_t value_length,
    size_t time_start,
    uint64_t *out_hours,
    uint64_t *out_minutes,
    uint64_t *out_seconds
);
static bool timestamp_time_total_seconds(
    uint64_t days,
    uint64_t hours,
    uint64_t minutes,
    uint64_t seconds,
    uint64_t *out_total,
    bool *out_clipped
);
static bool parse_timestamp_time_unsigned(
    const char *value,
    size_t start,
    size_t end,
    uint64_t *out_value
);
static bool timestamp_time_checked_add(uint64_t left, uint64_t right, uint64_t *out_value);
static bool timestamp_time_checked_mul(uint64_t left, uint64_t right, uint64_t *out_value);
static int append_timestamp_time_warning(
    struct mylite_db *database,
    const char *value,
    size_t value_length
);

int mylite_timestamp_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    enum mylite_date_interval_second_input_kind input_kind,
    bool value_is_null,
    const char *time_value,
    size_t time_value_length,
    bool time_value_is_null,
    bool has_time_value,
    char **out_text,
    bool *out_is_null
) {
    int64_t interval_seconds = 0;
    bool interval_is_null = false;
    char *base_text = NULL;
    bool base_is_null = false;
    int rc = MYLITE_OK;

    if (out_text == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_is_null = false;

    rc = mylite_date_interval_second_value_with_overflow_message(
        database,
        value,
        value_length,
        input_kind,
        value_is_null,
        0,
        false,
        false,
        "Datetime function: add_time field overflow",
        &base_text,
        &base_is_null
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (base_is_null) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (!has_time_value) {
        *out_text = base_text;
        return MYLITE_OK;
    }
    if (time_value_is_null) {
        free(base_text);
        *out_is_null = true;
        return MYLITE_OK;
    }
    rc = timestamp_time_argument(
        database,
        time_value,
        time_value_length,
        &interval_seconds,
        &interval_is_null
    );
    if (rc != MYLITE_OK) {
        free(base_text);
        return rc;
    }
    if (interval_is_null) {
        free(base_text);
        *out_is_null = true;
        return MYLITE_OK;
    }
    rc = mylite_date_interval_second_value_with_overflow_message(
        database,
        base_text,
        strlen(base_text),
        MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING,
        false,
        interval_seconds,
        false,
        false,
        "Datetime function: add_time field overflow",
        out_text,
        out_is_null
    );
    free(base_text);
    return rc;
}

int mylite_sqlite_register_timestamp_function(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_timestamp",
            .argument_count = 4,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = timestamp_sqlite_callback,
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

static void timestamp_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    enum mylite_date_interval_second_input_kind input_kind =
        MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING;
    const char *value = NULL;
    const char *time_value = NULL;
    size_t value_length = 0U;
    size_t time_value_length = 0U;
    bool value_is_null = false;
    bool time_value_is_null = false;
    bool has_time_value = false;
    bool result_is_null = false;
    char *result = NULL;
    int rc = MYLITE_OK;

    if (database == NULL || context == NULL || argc != 4 || argv == NULL || argv[0] == NULL ||
        argv[1] == NULL || argv[2] == NULL || argv[3] == NULL) {
        sqlite3_result_error(context, "invalid MyLite TIMESTAMP callback", -1);
        return;
    }
    rc = timestamp_sqlite_input_kind(context, argv[1], &input_kind);
    if (rc == MYLITE_OK) {
        rc = timestamp_sqlite_text_value(context, argv[0], &value, &value_length, &value_is_null);
    }
    if (rc == MYLITE_OK) {
        rc = timestamp_sqlite_text_value(
            context,
            argv[2],
            &time_value,
            &time_value_length,
            &time_value_is_null
        );
    }
    if (rc == MYLITE_OK) {
        has_time_value = false;
        if (sqlite3_value_type(argv[3]) != SQLITE_NULL && sqlite3_value_int64(argv[3]) != 0) {
            has_time_value = true;
        }
        rc = mylite_timestamp_value(
            database,
            value,
            value_length,
            input_kind,
            value_is_null,
            time_value,
            time_value_length,
            time_value_is_null,
            has_time_value,
            &result,
            &result_is_null
        );
    }
    (void)timestamp_sqlite_result(context, result, result_is_null, rc);
}

static int timestamp_sqlite_input_kind(
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
        sqlite3_result_error(context, "invalid MyLite TIMESTAMP input kind", -1);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int timestamp_sqlite_text_value(
    sqlite3_context *context,
    sqlite3_value *value,
    const char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    const unsigned char *text = NULL;
    int bytes = 0;

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
    bytes = sqlite3_value_bytes(value);
    if (text == NULL || bytes < 0) {
        sqlite3_result_error_nomem(context);
        return MYLITE_NOMEM;
    }
    *out_text = (const char *)text;
    *out_text_length = (size_t)bytes;
    return MYLITE_OK;
}

static int timestamp_sqlite_result(sqlite3_context *context, char *result, bool is_null, int rc) {
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
        } else {
            sqlite3_result_error(context, "MyLite TIMESTAMP() callback failed", -1);
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

static int timestamp_time_argument(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    int64_t *out_seconds,
    bool *out_is_null
) {
    bool clipped = false;

    if (out_seconds == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_seconds = 0;
    *out_is_null = false;
    if (!parse_timestamp_time_text(value, value_length, out_seconds, &clipped)) {
        int rc = append_timestamp_time_warning(database, value, value_length);

        if (rc == MYLITE_OK) {
            *out_is_null = true;
        }
        return rc;
    }
    if (clipped) {
        return append_timestamp_time_warning(database, value, value_length);
    }
    return MYLITE_OK;
}

static bool parse_timestamp_time_text(
    const char *value,
    size_t value_length,
    int64_t *out_seconds,
    bool *out_clipped
) {
    size_t index = 0U;
    size_t time_start = 0U;
    uint64_t days = 0U;
    uint64_t hours = 0U;
    uint64_t minutes = 0U;
    uint64_t seconds = 0U;
    uint64_t total = 0U;
    bool negative = false;

    if (value == NULL || out_seconds == NULL || out_clipped == NULL || value_length == 0U) {
        return false;
    }
    *out_seconds = 0;
    *out_clipped = false;
    if (!parse_timestamp_time_sign(value, value_length, &index, &negative)) {
        return false;
    }
    time_start = index;
    if (!parse_timestamp_time_day_prefix(value, value_length, &time_start, &days)) {
        return false;
    }
    if (!parse_timestamp_time_components(
            value,
            value_length,
            time_start,
            &hours,
            &minutes,
            &seconds
        )) {
        return false;
    }
    if (!timestamp_time_total_seconds(days, hours, minutes, seconds, &total, out_clipped)) {
        return false;
    }
    *out_seconds = (int64_t)total;
    if (negative) {
        *out_seconds = -*out_seconds;
    }
    return true;
}

static bool parse_timestamp_time_sign(
    const char *value,
    size_t value_length,
    size_t *inout_index,
    bool *out_negative
) {
    if (value == NULL || inout_index == NULL || out_negative == NULL ||
        *inout_index >= value_length) {
        return false;
    }
    *out_negative = false;
    if (value[*inout_index] != '+' && value[*inout_index] != '-') {
        return true;
    }
    if (value[*inout_index] == '-') {
        *out_negative = true;
    }
    ++(*inout_index);
    if (*inout_index >= value_length) {
        return false;
    }
    return true;
}

static bool parse_timestamp_time_day_prefix(
    const char *value,
    size_t value_length,
    size_t *inout_time_start,
    uint64_t *out_days
) {
    size_t day_start = 0U;

    if (value == NULL || inout_time_start == NULL || out_days == NULL) {
        return false;
    }
    day_start = *inout_time_start;
    *out_days = 0U;
    for (size_t current = day_start; current < value_length; ++current) {
        if (value[current] != ' ') {
            continue;
        }
        if (current == day_start || current + 1U >= value_length) {
            return false;
        }
        if (!parse_timestamp_time_unsigned(value, day_start, current, out_days)) {
            return false;
        }
        *inout_time_start = current + 1U;
        return true;
    }
    return true;
}

static bool parse_timestamp_time_components(
    const char *value,
    size_t value_length,
    size_t time_start,
    uint64_t *out_hours,
    uint64_t *out_minutes,
    uint64_t *out_seconds
) {
    size_t first_colon = time_start;
    size_t second_colon = 0U;

    while (first_colon < value_length && value[first_colon] != ':') {
        ++first_colon;
    }
    second_colon = first_colon + 1U;
    while (second_colon < value_length && value[second_colon] != ':') {
        ++second_colon;
    }

    if (out_hours == NULL || out_minutes == NULL || out_seconds == NULL ||
        first_colon == time_start || first_colon >= value_length || second_colon >= value_length ||
        second_colon + 3U != value_length || second_colon - first_colon != 3U) {
        return false;
    }
    if (!parse_timestamp_time_unsigned(value, time_start, first_colon, out_hours) ||
        !parse_timestamp_time_unsigned(value, first_colon + 1U, second_colon, out_minutes) ||
        !parse_timestamp_time_unsigned(value, second_colon + 1U, value_length, out_seconds)) {
        return false;
    }
    if (*out_minutes > timestamp_time_minute_second_max) {
        return false;
    }
    if (*out_seconds > timestamp_time_minute_second_max) {
        return false;
    }
    return true;
}

static bool timestamp_time_total_seconds(
    uint64_t days,
    uint64_t hours,
    uint64_t minutes,
    uint64_t seconds,
    uint64_t *out_total,
    bool *out_clipped
) {
    uint64_t total = 0U;

    if (out_total == NULL || out_clipped == NULL) {
        return false;
    }
    *out_total = 0U;
    *out_clipped = false;
    if (!timestamp_time_checked_mul(days, timestamp_time_hours_per_day, &total) ||
        !timestamp_time_checked_add(total, hours, &total) ||
        !timestamp_time_checked_mul(
            total,
            (uint64_t)timestamp_time_minutes_per_hour * (uint64_t)timestamp_time_seconds_per_minute,
            &total
        ) ||
        !timestamp_time_checked_add(
            total,
            minutes * (uint64_t)timestamp_time_seconds_per_minute,
            &total
        ) ||
        !timestamp_time_checked_add(total, seconds, &total)) {
        total = (uint64_t)timestamp_time_max_seconds;
        *out_clipped = true;
    }
    if (total > (uint64_t)timestamp_time_max_seconds) {
        total = (uint64_t)timestamp_time_max_seconds;
        *out_clipped = true;
    }
    *out_total = total;
    return true;
}

static bool parse_timestamp_time_unsigned(
    const char *value,
    size_t start,
    size_t end,
    uint64_t *out_value
) {
    uint64_t parsed = 0U;

    if (value == NULL || out_value == NULL || start >= end) {
        return false;
    }
    for (size_t index = start; index < end; ++index) {
        uint64_t next = 0U;

        if (value[index] < '0' || value[index] > '9') {
            return false;
        }
        if (!timestamp_time_checked_mul(parsed, timestamp_time_digit_radix, &next) ||
            !timestamp_time_checked_add(next, (uint64_t)(value[index] - '0'), &parsed)) {
            *out_value = UINT64_MAX;
            return true;
        }
    }
    *out_value = parsed;
    return true;
}

static bool timestamp_time_checked_add(uint64_t left, uint64_t right, uint64_t *out_value) {
    if (out_value == NULL || left > UINT64_MAX - right) {
        return false;
    }
    *out_value = left + right;
    return true;
}

static bool timestamp_time_checked_mul(uint64_t left, uint64_t right, uint64_t *out_value) {
    if (out_value == NULL || (right != 0U && left > UINT64_MAX / right)) {
        return false;
    }
    *out_value = left * right;
    return true;
}

static int append_timestamp_time_warning(
    struct mylite_db *database,
    const char *value,
    size_t value_length
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Truncated incorrect time value: '%.*s'",
        value_length > timestamp_time_value_preview_length ? timestamp_time_value_preview_length
                                                           : (int)value_length,
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
