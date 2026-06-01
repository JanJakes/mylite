#include "mylite_period_functions.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum {
    mysql_error_internal = 1105,
    mysql_error_incorrect_arguments = 1210,
    period_digits_base = 100,
    period_months_per_year = 12,
    period_month_min = 1,
    period_month_max = 12,
    period_two_digit_year_limit = 100,
    period_two_digit_year_cutoff = 70,
    period_two_digit_year_low_base = 2000,
    period_two_digit_year_high_base = 1900,
};

static void period_add_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void period_diff_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static int period_to_month_index(
    struct mylite_db *database,
    int64_t period,
    const char *function_name,
    int64_t *out_month_index
);
static int format_period_from_month_index(
    struct mylite_db *database,
    int64_t month_index,
    const char *function_name,
    int64_t *out_period
);
static int set_period_incorrect_argument_error(
    struct mylite_db *database,
    const char *function_name
);
static void period_sqlite_result(
    sqlite3_context *context,
    int64_t value,
    bool is_null,
    int rc,
    const char *message
);

int mylite_period_add_value(
    struct mylite_db *database,
    int64_t period,
    bool period_is_null,
    int64_t months,
    bool months_is_null,
    int64_t *out_value,
    bool *out_is_null
) {
    int64_t month_index = 0;
    int64_t result_month_index = 0;
    int rc = MYLITE_OK;

    if (out_value == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0;
    *out_is_null = false;
    if (period_is_null || months_is_null) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    rc = period_to_month_index(database, period, "period_add", &month_index);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if ((months > 0 && month_index > INT64_MAX - months) ||
        (months < 0 && month_index < INT64_MIN - months)) {
        return set_period_incorrect_argument_error(database, "period_add");
    }
    result_month_index = month_index + months;
    return format_period_from_month_index(database, result_month_index, "period_add", out_value);
}

int mylite_period_diff_value(
    struct mylite_db *database,
    int64_t period1,
    bool period1_is_null,
    int64_t period2,
    bool period2_is_null,
    int64_t *out_value,
    bool *out_is_null
) {
    int64_t left_month_index = 0;
    int64_t right_month_index = 0;
    int rc = MYLITE_OK;

    if (out_value == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0;
    *out_is_null = false;
    if (period1_is_null || period2_is_null) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    rc = period_to_month_index(database, period1, "period_diff", &left_month_index);
    if (rc == MYLITE_OK) {
        rc = period_to_month_index(database, period2, "period_diff", &right_month_index);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if ((right_month_index < 0 && left_month_index > INT64_MAX + right_month_index) ||
        (right_month_index > 0 && left_month_index < INT64_MIN + right_month_index)) {
        return set_period_incorrect_argument_error(database, "period_diff");
    }

    *out_value = left_month_index - right_month_index;
    return MYLITE_OK;
}

int mylite_sqlite_register_period_functions(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_period_add",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = period_add_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_period_diff",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = period_diff_sqlite_callback,
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

static void period_add_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    int64_t value = 0;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (database == NULL || argc != 2 || argv == NULL) {
        sqlite3_result_error(context, "MyLite PERIOD_ADD() callback failed", -1);
        return;
    }
    rc = mylite_period_add_value(
        database,
        sqlite3_value_int64(argv[0]),
        sqlite3_value_type(argv[0]) == SQLITE_NULL,
        sqlite3_value_int64(argv[1]),
        sqlite3_value_type(argv[1]) == SQLITE_NULL,
        &value,
        &is_null
    );
    period_sqlite_result(context, value, is_null, rc, "Incorrect arguments to period_add");
}

static void period_diff_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    int64_t value = 0;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (database == NULL || argc != 2 || argv == NULL) {
        sqlite3_result_error(context, "MyLite PERIOD_DIFF() callback failed", -1);
        return;
    }
    rc = mylite_period_diff_value(
        database,
        sqlite3_value_int64(argv[0]),
        sqlite3_value_type(argv[0]) == SQLITE_NULL,
        sqlite3_value_int64(argv[1]),
        sqlite3_value_type(argv[1]) == SQLITE_NULL,
        &value,
        &is_null
    );
    period_sqlite_result(context, value, is_null, rc, "Incorrect arguments to period_diff");
}

static int period_to_month_index(
    struct mylite_db *database,
    int64_t period,
    const char *function_name,
    int64_t *out_month_index
) {
    int64_t year = 0;
    int64_t month = 0;

    if (out_month_index == NULL) {
        return MYLITE_MISUSE;
    }
    *out_month_index = 0;
    if (period <= 0) {
        return set_period_incorrect_argument_error(database, function_name);
    }

    year = period / period_digits_base;
    month = period % period_digits_base;
    if (month < period_month_min || month > period_month_max) {
        return set_period_incorrect_argument_error(database, function_name);
    }
    if (year < period_two_digit_year_limit) {
        year += year < period_two_digit_year_cutoff ? period_two_digit_year_low_base
                                                    : period_two_digit_year_high_base;
    }
    if (year > (INT64_MAX - (period_months_per_year - 1)) / period_months_per_year) {
        return set_period_incorrect_argument_error(database, function_name);
    }

    *out_month_index = (year * period_months_per_year) + (month - 1);
    return MYLITE_OK;
}

static int format_period_from_month_index(
    struct mylite_db *database,
    int64_t month_index,
    const char *function_name,
    int64_t *out_period
) {
    int64_t year = 0;
    int64_t month = 0;

    if (out_period == NULL) {
        return MYLITE_MISUSE;
    }
    if (month_index < 0) {
        return set_period_incorrect_argument_error(database, function_name);
    }
    year = month_index / period_months_per_year;
    month = (month_index % period_months_per_year) + 1;
    if (year > (INT64_MAX - month) / period_digits_base) {
        return set_period_incorrect_argument_error(database, function_name);
    }
    *out_period = (year * period_digits_base) + month;
    return MYLITE_OK;
}

static int set_period_incorrect_argument_error(
    struct mylite_db *database,
    const char *function_name
) {
    const char *name = function_name == NULL ? "period" : function_name;
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Incorrect arguments to %s", name);

    if (database == NULL) {
        return MYLITE_ERROR;
    }
    if (written < 0 || (size_t)written >= sizeof(message)) {
        mylite_diagnostics_set_error(
            &database->diagnostics,
            mysql_error_internal,
            "HY000",
            "failed to format period function error"
        );
        return MYLITE_ERROR;
    }
    mylite_diagnostics_set_error(
        &database->diagnostics,
        mysql_error_incorrect_arguments,
        "HY000",
        message
    );
    return MYLITE_ERROR;
}

static void period_sqlite_result(
    sqlite3_context *context,
    int64_t value,
    bool is_null,
    int rc,
    const char *message
) {
    if (rc != MYLITE_OK) {
        sqlite3_result_error(context, message, -1);
        return;
    }
    if (is_null) {
        sqlite3_result_null(context);
        return;
    }
    sqlite3_result_int64(context, value);
}
