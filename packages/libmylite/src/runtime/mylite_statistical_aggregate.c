#include "mylite_statistical_aggregate.h"

#include "mylite_sqlite_registration.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

enum mylite_statistical_aggregate_operation {
    MYLITE_STATISTICAL_AGGREGATE_STDDEV_POP = 0,
    MYLITE_STATISTICAL_AGGREGATE_STDDEV_SAMP = 1,
    MYLITE_STATISTICAL_AGGREGATE_VAR_POP = 2,
    MYLITE_STATISTICAL_AGGREGATE_VAR_SAMP = 3,
};

struct mylite_statistical_aggregate_config {
    enum mylite_statistical_aggregate_operation operation;
};

struct mylite_statistical_aggregate_state {
    uint64_t count;
    double mean;
    double m2;
};

static const double statistical_aggregate_variance_zero_epsilon = 1.0e-12;

static void statistical_aggregate_step(sqlite3_context *context, int argc, sqlite3_value **argv);
static void statistical_aggregate_final(sqlite3_context *context);
static const struct mylite_statistical_aggregate_config *statistical_aggregate_config(
    sqlite3_context *context
);
static bool statistical_aggregate_is_sample(const struct mylite_statistical_aggregate_config *config
);
static bool statistical_aggregate_is_stddev(const struct mylite_statistical_aggregate_config *config
);

int mylite_sqlite_register_statistical_aggregate_functions(sqlite3 *sqlite) {
    static struct mylite_statistical_aggregate_config stddev_pop_config = {
        .operation = MYLITE_STATISTICAL_AGGREGATE_STDDEV_POP,
    };
    static struct mylite_statistical_aggregate_config stddev_samp_config = {
        .operation = MYLITE_STATISTICAL_AGGREGATE_STDDEV_SAMP,
    };
    static struct mylite_statistical_aggregate_config var_pop_config = {
        .operation = MYLITE_STATISTICAL_AGGREGATE_VAR_POP,
    };
    static struct mylite_statistical_aggregate_config var_samp_config = {
        .operation = MYLITE_STATISTICAL_AGGREGATE_VAR_SAMP,
    };
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_AGGREGATE,
            .name = "_mylite_stddev_pop",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = &stddev_pop_config,
            .scalar_callback = NULL,
            .step_callback = statistical_aggregate_step,
            .final_callback = statistical_aggregate_final,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_AGGREGATE,
            .name = "_mylite_stddev_samp",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = &stddev_samp_config,
            .scalar_callback = NULL,
            .step_callback = statistical_aggregate_step,
            .final_callback = statistical_aggregate_final,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_AGGREGATE,
            .name = "_mylite_var_pop",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = &var_pop_config,
            .scalar_callback = NULL,
            .step_callback = statistical_aggregate_step,
            .final_callback = statistical_aggregate_final,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_AGGREGATE,
            .name = "_mylite_var_samp",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = &var_samp_config,
            .scalar_callback = NULL,
            .step_callback = statistical_aggregate_step,
            .final_callback = statistical_aggregate_final,
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

static void statistical_aggregate_step(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const struct mylite_statistical_aggregate_config *config =
        statistical_aggregate_config(context);
    struct mylite_statistical_aggregate_state *state = NULL;
    int value_type = SQLITE_NULL;
    double value = 0.0;
    double delta = 0.0;
    double delta2 = 0.0;

    if (config == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite statistical aggregate callback", -1);
        return;
    }

    value_type = sqlite3_value_type(argv[0]);
    if (value_type == SQLITE_NULL) {
        return;
    }
    if (value_type != SQLITE_INTEGER && value_type != SQLITE_FLOAT) {
        sqlite3_result_error(context, "invalid MyLite statistical aggregate input type", -1);
        return;
    }

    state = sqlite3_aggregate_context(context, (int)sizeof(*state));
    if (state == NULL) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (state->count == UINT64_MAX) {
        sqlite3_result_error(context, "MyLite statistical aggregate row count overflow", -1);
        return;
    }

    value = sqlite3_value_double(argv[0]);
    ++state->count;
    delta = value - state->mean;
    state->mean += delta / (double)state->count;
    delta2 = value - state->mean;
    state->m2 += delta * delta2;
}

static void statistical_aggregate_final(sqlite3_context *context) {
    const struct mylite_statistical_aggregate_config *config =
        statistical_aggregate_config(context);
    struct mylite_statistical_aggregate_state *state = sqlite3_aggregate_context(context, 0);
    double denominator = 0.0;
    double variance = 0.0;
    double result = 0.0;

    if (config == NULL) {
        sqlite3_result_error(context, "invalid MyLite statistical aggregate callback", -1);
        return;
    }
    if (state == NULL || state->count == 0U) {
        sqlite3_result_null(context);
        return;
    }
    if (statistical_aggregate_is_sample(config) && state->count < 2U) {
        sqlite3_result_null(context);
        return;
    }

    denominator = statistical_aggregate_is_sample(config) ? (double)(state->count - 1U)
                                                          : (double)state->count;
    variance = state->m2 / denominator;
    if (variance < 0.0 && variance > -statistical_aggregate_variance_zero_epsilon) {
        variance = 0.0;
    }
    result = statistical_aggregate_is_stddev(config) ? sqrt(variance) : variance;
    sqlite3_result_double(context, result == 0.0 ? 0.0 : result);
}

static const struct mylite_statistical_aggregate_config *statistical_aggregate_config(
    sqlite3_context *context
) {
    return sqlite3_user_data(context);
}

static bool statistical_aggregate_is_sample(const struct mylite_statistical_aggregate_config *config
) {
    return config->operation == MYLITE_STATISTICAL_AGGREGATE_STDDEV_SAMP ||
           config->operation == MYLITE_STATISTICAL_AGGREGATE_VAR_SAMP;
}

static bool statistical_aggregate_is_stddev(const struct mylite_statistical_aggregate_config *config
) {
    return config->operation == MYLITE_STATISTICAL_AGGREGATE_STDDEV_POP ||
           config->operation == MYLITE_STATISTICAL_AGGREGATE_STDDEV_SAMP;
}
