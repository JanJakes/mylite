#include "mylite_numeric_functions.h"

#include "mylite_sqlite_registration.h"

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

enum mylite_numeric_function_operation {
    MYLITE_NUMERIC_OPERATION_NONE = 0,
    MYLITE_NUMERIC_OPERATION_ABS = 1,
    MYLITE_NUMERIC_OPERATION_SIGN = 2,
    MYLITE_NUMERIC_OPERATION_CEIL = 3,
    MYLITE_NUMERIC_OPERATION_FLOOR = 4,
    MYLITE_NUMERIC_OPERATION_ROUND = 5,
    MYLITE_NUMERIC_OPERATION_SQRT = 6,
    MYLITE_NUMERIC_OPERATION_DEGREES = 7,
    MYLITE_NUMERIC_OPERATION_RADIANS = 8,
    MYLITE_NUMERIC_OPERATION_ACOS = 9,
    MYLITE_NUMERIC_OPERATION_ASIN = 10,
    MYLITE_NUMERIC_OPERATION_ATAN = 11,
    MYLITE_NUMERIC_OPERATION_SIN = 12,
    MYLITE_NUMERIC_OPERATION_COS = 13,
    MYLITE_NUMERIC_OPERATION_TAN = 14,
    MYLITE_NUMERIC_OPERATION_COT = 15,
    MYLITE_NUMERIC_OPERATION_EXP = 16,
    MYLITE_NUMERIC_OPERATION_LN = 17,
    MYLITE_NUMERIC_OPERATION_LOG = 18,
    MYLITE_NUMERIC_OPERATION_LOG10 = 19,
    MYLITE_NUMERIC_OPERATION_LOG2 = 20,
    MYLITE_NUMERIC_OPERATION_POW = 21,
    MYLITE_NUMERIC_OPERATION_BIT_COUNT = 22,
};

struct numeric_round_request {
    double value;
    int64_t places;
};

static const enum mylite_numeric_function_operation numeric_operations[] = {
    MYLITE_NUMERIC_OPERATION_NONE,      MYLITE_NUMERIC_OPERATION_ABS,
    MYLITE_NUMERIC_OPERATION_SIGN,      MYLITE_NUMERIC_OPERATION_CEIL,
    MYLITE_NUMERIC_OPERATION_FLOOR,     MYLITE_NUMERIC_OPERATION_ROUND,
    MYLITE_NUMERIC_OPERATION_SQRT,      MYLITE_NUMERIC_OPERATION_DEGREES,
    MYLITE_NUMERIC_OPERATION_RADIANS,   MYLITE_NUMERIC_OPERATION_ACOS,
    MYLITE_NUMERIC_OPERATION_ASIN,      MYLITE_NUMERIC_OPERATION_ATAN,
    MYLITE_NUMERIC_OPERATION_SIN,       MYLITE_NUMERIC_OPERATION_COS,
    MYLITE_NUMERIC_OPERATION_TAN,       MYLITE_NUMERIC_OPERATION_COT,
    MYLITE_NUMERIC_OPERATION_EXP,       MYLITE_NUMERIC_OPERATION_LN,
    MYLITE_NUMERIC_OPERATION_LOG,       MYLITE_NUMERIC_OPERATION_LOG10,
    MYLITE_NUMERIC_OPERATION_LOG2,      MYLITE_NUMERIC_OPERATION_POW,
    MYLITE_NUMERIC_OPERATION_BIT_COUNT,
};

static const double degrees_per_half_turn = 180.0;
static const double int64_max_plus_one_as_double = 9223372036854775808.0;
static const double int64_min_as_double = -9223372036854775808.0;
static const double logarithm_base_two = 2.0;
static const double logarithm_base_ten = 10.0;
static const int64_t round_scale_place_limit = 308;

static void numeric_function_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static bool numeric_function_operation_from_context(
    sqlite3_context *context,
    enum mylite_numeric_function_operation *out_operation
);
static void numeric_function_result(
    sqlite3_context *context,
    double value,
    bool use_integer_when_exact
);
static void numeric_function_finite_result_or_error(
    sqlite3_context *context,
    double value,
    bool use_integer_when_exact
);
static void numeric_abs_result(sqlite3_context *context, sqlite3_value *value);
static void numeric_sign_result(sqlite3_context *context, sqlite3_value *value);
static void numeric_round_result(sqlite3_context *context, int argc, sqlite3_value **argv);
static void numeric_unary_result(
    sqlite3_context *context,
    enum mylite_numeric_function_operation operation,
    sqlite3_value *value
);
static void numeric_binary_result(
    sqlite3_context *context,
    enum mylite_numeric_function_operation operation,
    sqlite3_value *left,
    sqlite3_value *right
);
static int64_t numeric_round_places(sqlite3_value *value);
static double round_to_places(struct numeric_round_request request);
static int bit_count_u64(uint64_t value);

int mylite_sqlite_register_numeric_functions(sqlite3 *sqlite) {
    enum { flags = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC };
    static const struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_abs",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_ABS],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_sign",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_SIGN],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_ceil",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_CEIL],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_ceiling",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_CEIL],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_floor",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_FLOOR],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_round",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_ROUND],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_round",
            .argument_count = 2,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_ROUND],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_sqrt",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_SQRT],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_degrees",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_DEGREES],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_radians",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_RADIANS],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_acos",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_ACOS],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_asin",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_ASIN],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_atan",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_ATAN],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_atan",
            .argument_count = 2,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_ATAN],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_atan2",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_ATAN],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_atan2",
            .argument_count = 2,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_ATAN],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_sin",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_SIN],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_cos",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_COS],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_tan",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_TAN],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_cot",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_COT],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_exp",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_EXP],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_ln",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_LN],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_log",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_LOG],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_log",
            .argument_count = 2,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_LOG],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_log10",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_LOG10],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_log2",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_LOG2],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_pow",
            .argument_count = 2,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_POW],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_power",
            .argument_count = 2,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_POW],
            .scalar_callback = numeric_function_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_numeric_bit_count",
            .argument_count = 1,
            .text_representation = flags,
            .application_data = (void *)&numeric_operations[MYLITE_NUMERIC_OPERATION_BIT_COUNT],
            .scalar_callback = numeric_function_sqlite_callback,
        },
    };

    return mylite_sqlite_register_functions(
        sqlite,
        registrations,
        sizeof(registrations) / sizeof(registrations[0])
    );
}

static void numeric_function_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    enum mylite_numeric_function_operation operation = MYLITE_NUMERIC_OPERATION_NONE;

    if (context == NULL || argc < 1 || argc > 2 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite numeric function callback", -1);
        return;
    }
    if (!numeric_function_operation_from_context(context, &operation)) {
        sqlite3_result_error(context, "invalid MyLite numeric function operation", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL ||
        (argc == 2 && sqlite3_value_type(argv[1]) == SQLITE_NULL)) {
        sqlite3_result_null(context);
        return;
    }

    switch (operation) {
    case MYLITE_NUMERIC_OPERATION_ABS:
        numeric_abs_result(context, argv[0]);
        return;
    case MYLITE_NUMERIC_OPERATION_SIGN:
        numeric_sign_result(context, argv[0]);
        return;
    case MYLITE_NUMERIC_OPERATION_ROUND:
        numeric_round_result(context, argc, argv);
        return;
    case MYLITE_NUMERIC_OPERATION_CEIL:
    case MYLITE_NUMERIC_OPERATION_FLOOR:
    case MYLITE_NUMERIC_OPERATION_SQRT:
    case MYLITE_NUMERIC_OPERATION_DEGREES:
    case MYLITE_NUMERIC_OPERATION_RADIANS:
    case MYLITE_NUMERIC_OPERATION_ACOS:
    case MYLITE_NUMERIC_OPERATION_ASIN:
    case MYLITE_NUMERIC_OPERATION_SIN:
    case MYLITE_NUMERIC_OPERATION_COS:
    case MYLITE_NUMERIC_OPERATION_TAN:
    case MYLITE_NUMERIC_OPERATION_COT:
    case MYLITE_NUMERIC_OPERATION_EXP:
    case MYLITE_NUMERIC_OPERATION_LN:
    case MYLITE_NUMERIC_OPERATION_LOG10:
    case MYLITE_NUMERIC_OPERATION_LOG2:
    case MYLITE_NUMERIC_OPERATION_BIT_COUNT:
        if (argc != 1) {
            sqlite3_result_error(context, "invalid MyLite numeric unary function arity", -1);
            return;
        }
        numeric_unary_result(context, operation, argv[0]);
        return;
    case MYLITE_NUMERIC_OPERATION_ATAN:
    case MYLITE_NUMERIC_OPERATION_LOG:
    case MYLITE_NUMERIC_OPERATION_POW:
        if (argc == 1) {
            numeric_unary_result(context, operation, argv[0]);
            return;
        }
        if (argv[1] == NULL) {
            sqlite3_result_error(context, "invalid MyLite numeric binary function argument", -1);
            return;
        }
        numeric_binary_result(context, operation, argv[0], argv[1]);
        return;
    case MYLITE_NUMERIC_OPERATION_NONE:
        break;
    }

    sqlite3_result_error(context, "unsupported MyLite numeric function", -1);
}

static bool numeric_function_operation_from_context(
    sqlite3_context *context,
    enum mylite_numeric_function_operation *out_operation
) {
    const enum mylite_numeric_function_operation *operation = NULL;

    if (context == NULL || out_operation == NULL) {
        return false;
    }
    operation = sqlite3_user_data(context);
    if (operation == NULL || *operation <= MYLITE_NUMERIC_OPERATION_NONE ||
        *operation > MYLITE_NUMERIC_OPERATION_BIT_COUNT) {
        return false;
    }
    *out_operation = *operation;
    return true;
}

static void numeric_abs_result(sqlite3_context *context, sqlite3_value *value) {
    if (sqlite3_value_numeric_type(value) == SQLITE_INTEGER) {
        sqlite3_int64 integer = sqlite3_value_int64(value);

        if (integer == (sqlite3_int64)LLONG_MIN) {
            sqlite3_result_error(context, "BIGINT value is out of range in 'abs(...)'", -1);
            return;
        }
        if (integer < 0) {
            integer = -integer;
        }
        sqlite3_result_int64(context, integer);
        return;
    }

    numeric_function_finite_result_or_error(context, fabs(sqlite3_value_double(value)), true);
}

static void numeric_sign_result(sqlite3_context *context, sqlite3_value *value) {
    double input = sqlite3_value_double(value);

    if (input > 0.0) {
        sqlite3_result_int64(context, 1);
    } else if (input < 0.0) {
        sqlite3_result_int64(context, -1);
    } else {
        sqlite3_result_int64(context, 0);
    }
}

static void numeric_round_result(sqlite3_context *context, int argc, sqlite3_value **argv) {
    double input = sqlite3_value_double(argv[0]);
    int64_t places = 0;

    if (argc == 2) {
        places = numeric_round_places(argv[1]);
    }
    numeric_function_finite_result_or_error(
        context,
        round_to_places((struct numeric_round_request){.value = input, .places = places}),
        true
    );
}

static void numeric_unary_result(
    sqlite3_context *context,
    enum mylite_numeric_function_operation operation,
    sqlite3_value *value
) {
    double input = sqlite3_value_double(value);
    double pi = acos(-1.0);

    switch (operation) {
    case MYLITE_NUMERIC_OPERATION_CEIL:
        numeric_function_finite_result_or_error(context, ceil(input), true);
        return;
    case MYLITE_NUMERIC_OPERATION_FLOOR:
        numeric_function_finite_result_or_error(context, floor(input), true);
        return;
    case MYLITE_NUMERIC_OPERATION_SQRT:
        if (input < 0.0) {
            sqlite3_result_null(context);
            return;
        }
        numeric_function_finite_result_or_error(context, sqrt(input), true);
        return;
    case MYLITE_NUMERIC_OPERATION_DEGREES:
        numeric_function_finite_result_or_error(
            context,
            input * (degrees_per_half_turn / pi),
            false
        );
        return;
    case MYLITE_NUMERIC_OPERATION_RADIANS:
        numeric_function_finite_result_or_error(
            context,
            input * (pi / degrees_per_half_turn),
            false
        );
        return;
    case MYLITE_NUMERIC_OPERATION_ACOS:
        if (input < -1.0 || input > 1.0) {
            sqlite3_result_null(context);
            return;
        }
        numeric_function_finite_result_or_error(context, acos(input), false);
        return;
    case MYLITE_NUMERIC_OPERATION_ASIN:
        if (input < -1.0 || input > 1.0) {
            sqlite3_result_null(context);
            return;
        }
        numeric_function_finite_result_or_error(context, asin(input), false);
        return;
    case MYLITE_NUMERIC_OPERATION_ATAN:
        numeric_function_finite_result_or_error(context, atan(input), false);
        return;
    case MYLITE_NUMERIC_OPERATION_SIN:
        numeric_function_finite_result_or_error(context, sin(input), false);
        return;
    case MYLITE_NUMERIC_OPERATION_COS:
        numeric_function_finite_result_or_error(context, cos(input), false);
        return;
    case MYLITE_NUMERIC_OPERATION_TAN:
        numeric_function_finite_result_or_error(context, tan(input), false);
        return;
    case MYLITE_NUMERIC_OPERATION_COT:
        if (input == 0.0) {
            sqlite3_result_error(context, "DOUBLE value is out of range in 'cot(0)'", -1);
            return;
        }
        numeric_function_finite_result_or_error(context, 1.0 / tan(input), false);
        return;
    case MYLITE_NUMERIC_OPERATION_EXP:
        numeric_function_finite_result_or_error(context, exp(input), false);
        return;
    case MYLITE_NUMERIC_OPERATION_LN:
    case MYLITE_NUMERIC_OPERATION_LOG:
        if (input <= 0.0) {
            sqlite3_result_null(context);
            return;
        }
        numeric_function_finite_result_or_error(context, log(input), false);
        return;
    case MYLITE_NUMERIC_OPERATION_LOG10:
        if (input <= 0.0) {
            sqlite3_result_null(context);
            return;
        }
        numeric_function_finite_result_or_error(context, log10(input), false);
        return;
    case MYLITE_NUMERIC_OPERATION_LOG2:
        if (input <= 0.0) {
            sqlite3_result_null(context);
            return;
        }
        numeric_function_finite_result_or_error(
            context,
            log(input) / log(logarithm_base_two),
            false
        );
        return;
    case MYLITE_NUMERIC_OPERATION_BIT_COUNT:
        sqlite3_result_int64(context, bit_count_u64((uint64_t)sqlite3_value_int64(value)));
        return;
    case MYLITE_NUMERIC_OPERATION_ABS:
    case MYLITE_NUMERIC_OPERATION_SIGN:
    case MYLITE_NUMERIC_OPERATION_ROUND:
    case MYLITE_NUMERIC_OPERATION_POW:
    case MYLITE_NUMERIC_OPERATION_NONE:
        break;
    }

    sqlite3_result_error(context, "unsupported MyLite numeric unary function", -1);
}

static void numeric_binary_result(
    sqlite3_context *context,
    enum mylite_numeric_function_operation operation,
    sqlite3_value *left,
    sqlite3_value *right
) {
    double first = sqlite3_value_double(left);
    double second = sqlite3_value_double(right);

    switch (operation) {
    case MYLITE_NUMERIC_OPERATION_ATAN:
        numeric_function_finite_result_or_error(context, atan2(first, second), false);
        return;
    case MYLITE_NUMERIC_OPERATION_LOG:
        if (first <= 0.0 || first == 1.0 || second <= 0.0) {
            sqlite3_result_null(context);
            return;
        }
        numeric_function_finite_result_or_error(context, log(second) / log(first), false);
        return;
    case MYLITE_NUMERIC_OPERATION_POW:
        numeric_function_finite_result_or_error(context, pow(first, second), false);
        return;
    case MYLITE_NUMERIC_OPERATION_NONE:
    case MYLITE_NUMERIC_OPERATION_ABS:
    case MYLITE_NUMERIC_OPERATION_SIGN:
    case MYLITE_NUMERIC_OPERATION_CEIL:
    case MYLITE_NUMERIC_OPERATION_FLOOR:
    case MYLITE_NUMERIC_OPERATION_ROUND:
    case MYLITE_NUMERIC_OPERATION_SQRT:
    case MYLITE_NUMERIC_OPERATION_DEGREES:
    case MYLITE_NUMERIC_OPERATION_RADIANS:
    case MYLITE_NUMERIC_OPERATION_ACOS:
    case MYLITE_NUMERIC_OPERATION_ASIN:
    case MYLITE_NUMERIC_OPERATION_SIN:
    case MYLITE_NUMERIC_OPERATION_COS:
    case MYLITE_NUMERIC_OPERATION_TAN:
    case MYLITE_NUMERIC_OPERATION_COT:
    case MYLITE_NUMERIC_OPERATION_EXP:
    case MYLITE_NUMERIC_OPERATION_LN:
    case MYLITE_NUMERIC_OPERATION_LOG10:
    case MYLITE_NUMERIC_OPERATION_LOG2:
    case MYLITE_NUMERIC_OPERATION_BIT_COUNT:
        break;
    }

    sqlite3_result_error(context, "unsupported MyLite numeric binary function", -1);
}

static int64_t numeric_round_places(sqlite3_value *value) {
    if (sqlite3_value_numeric_type(value) == SQLITE_INTEGER) {
        return (int64_t)sqlite3_value_int64(value);
    }
    return (int64_t)round(sqlite3_value_double(value));
}

static double round_to_places(struct numeric_round_request request) {
    double scale = 0.0;

    if (request.places == 0) {
        return round(request.value);
    }
    if (request.places > round_scale_place_limit) {
        return request.value;
    }
    if (request.places < -round_scale_place_limit) {
        return 0.0;
    }

    if (request.places > 0) {
        scale = pow(logarithm_base_ten, (double)request.places);
        return round(request.value * scale) / scale;
    }

    scale = pow(logarithm_base_ten, (double)-request.places);
    return round(request.value / scale) * scale;
}

static void numeric_function_finite_result_or_error(
    sqlite3_context *context,
    double value,
    bool use_integer_when_exact
) {
    if (!isfinite(value)) {
        sqlite3_result_error(context, "DOUBLE value is out of range", -1);
        return;
    }
    numeric_function_result(context, value, use_integer_when_exact);
}

static void numeric_function_result(
    sqlite3_context *context,
    double value,
    bool use_integer_when_exact
) {
    double integer_value = trunc(value);

    if (use_integer_when_exact && integer_value == value && value >= int64_min_as_double &&
        value < int64_max_plus_one_as_double) {
        sqlite3_result_int64(context, (sqlite3_int64)value);
        return;
    }
    sqlite3_result_double(context, value);
}

static int bit_count_u64(uint64_t value) {
    int count = 0;

    while (value != 0U) {
        count += (int)(value & 1U);
        value >>= 1U;
    }
    return count;
}
