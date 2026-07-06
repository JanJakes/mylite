#include "mylite_numeric_functions.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_mysql_error_codes.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <mylite/mylite.h>

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

enum {
    numeric_double_parse_stack_capacity = 128,
    avg_order_prefix_digits = 1,
    avg_order_integer_digits = 19,
    avg_order_separator_digits = 1,
    avg_order_fraction_digits = 39,
    avg_order_fraction_offset =
        avg_order_prefix_digits + avg_order_integer_digits + avg_order_separator_digits,
    avg_order_key_capacity = avg_order_fraction_offset + avg_order_fraction_digits + 1,
    avg_window_result_fraction_digits = 4,
    avg_window_result_fraction_scale = 10000,
    avg_window_result_round_half_digit = 5,
    avg_window_result_decimal_base = 10,
    avg_window_result_capacity = 64,
};

struct numeric_round_request {
    double value;
    int64_t places;
    bool use_approximate_ties;
};

struct numeric_integer_parse {
    int64_t signed_value;
    uint64_t unsigned_value;
    bool truncated;
};

struct avg_order_accumulator {
    int64_t sum;
    int64_t count;
};

struct avg_order_uint128_parts {
    uint64_t high;
    uint64_t low;
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
static const double approximate_round_tie_fraction = 0.5;
static const double integral_even_modulus = 2.0;
static const int64_t round_scale_place_limit = 308;
static const uint64_t integer_parse_base = 10U;
static const uint64_t int64_min_magnitude = (uint64_t)INT64_MAX + 1U;
static const size_t numeric_warning_value_preview_length = 200U;

static void numeric_function_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static void avg_window_result_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static void avg_order_key_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
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
static int numeric_double_value(sqlite3_context *context, sqlite3_value *value, double *out_value);
static int numeric_round_places_value(
    sqlite3_context *context,
    sqlite3_value *value,
    int64_t *out_places
);
static int numeric_bit_count_value(
    sqlite3_context *context,
    sqlite3_value *value,
    uint64_t *out_value
);
static int numeric_text_double_value(
    sqlite3_context *context,
    const char *text,
    size_t text_length,
    double *out_value
);
static int numeric_text_integer_value(
    sqlite3_context *context,
    const char *text,
    size_t text_length,
    struct numeric_integer_parse *out_parse
);
static int numeric_value_text(sqlite3_value *value, const char **out_text, size_t *out_length);
static size_t numeric_double_prefix_length(
    const char *text,
    size_t text_length,
    bool *out_saw_digits
);
static bool numeric_text_has_nonspace_suffix(const char *text, size_t text_length, size_t offset);
static int append_truncated_incorrect_number_warning(
    sqlite3_context *context,
    const char *type_name,
    const char *value,
    size_t value_length
);
static int append_invalid_logarithm_warning(sqlite3_context *context);
static void numeric_sqlite_result_error_from_rc(
    sqlite3_context *context,
    int rc,
    const char *message
);
static bool numeric_round_value_uses_approximate_ties(sqlite3_value *value);
static double round_to_places(struct numeric_round_request request);
static double round_to_integral(double value, bool use_approximate_ties);
static int bit_count_u64(uint64_t value);
static bool format_avg_order_key(
    struct avg_order_accumulator accumulator,
    char *buffer,
    size_t buffer_size
);
static bool format_avg_window_result(
    struct avg_order_accumulator accumulator,
    char *buffer,
    size_t buffer_size
);
static uint64_t avg_order_absolute_int64_magnitude(int64_t value);
static int avg_order_next_decimal_digit(uint64_t *remainder, uint64_t denominator);
static struct avg_order_uint128_parts avg_order_multiply_u64_by_decimal_radix(uint64_t value);
static bool avg_order_uint128_ge_u64(const struct avg_order_uint128_parts *left, uint64_t right);
static void avg_order_uint128_subtract_u64(struct avg_order_uint128_parts *left, uint64_t right);

int mylite_sqlite_register_numeric_functions(sqlite3 *sqlite) {
    enum { flags = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS };
    static const struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_avg_window_result",
            .argument_count = 2,
            .text_representation = flags,
            .application_data = NULL,
            .scalar_callback = avg_window_result_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_avg_order_key",
            .argument_count = 2,
            .text_representation = flags,
            .application_data = NULL,
            .scalar_callback = avg_order_key_sqlite_callback,
        },
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

static void avg_window_result_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    char result[avg_window_result_capacity];
    int64_t sum = 0;
    int64_t count = 0;

    if (argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL) {
        sqlite3_result_error(context, "invalid MyLite AVG window callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[1]) != SQLITE_INTEGER) {
        sqlite3_result_error(context, "invalid MyLite AVG window count", -1);
        return;
    }
    count = (int64_t)sqlite3_value_int64(argv[1]);
    if (count < 0) {
        sqlite3_result_error(context, "invalid MyLite AVG window count", -1);
        return;
    }
    if (count == 0) {
        sqlite3_result_null(context);
        return;
    }
    if (sqlite3_value_type(argv[0]) != SQLITE_INTEGER) {
        sqlite3_result_error(context, "invalid MyLite AVG window sum", -1);
        return;
    }
    sum = (int64_t)sqlite3_value_int64(argv[0]);

    if (!format_avg_window_result(
            (struct avg_order_accumulator){
                .sum = sum,
                .count = count,
            },
            result,
            sizeof(result)
        )) {
        sqlite3_result_error(context, "failed to format MyLite AVG window result", -1);
        return;
    }

    sqlite3_result_text(context, result, -1, SQLITE_TRANSIENT);
}

static void avg_order_key_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    char key[avg_order_key_capacity];
    int64_t sum = 0;
    int64_t count = 0;

    if (argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL) {
        sqlite3_result_error(context, "invalid MyLite AVG order-key callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[1]) != SQLITE_INTEGER) {
        sqlite3_result_error(context, "invalid MyLite AVG order-key count", -1);
        return;
    }
    count = (int64_t)sqlite3_value_int64(argv[1]);
    if (count < 0) {
        sqlite3_result_error(context, "invalid MyLite AVG order-key count", -1);
        return;
    }
    if (count == 0) {
        sqlite3_result_null(context);
        return;
    }
    if (sqlite3_value_type(argv[0]) != SQLITE_INTEGER) {
        sqlite3_result_error(context, "invalid MyLite AVG order-key sum", -1);
        return;
    }
    sum = (int64_t)sqlite3_value_int64(argv[0]);

    if (!format_avg_order_key(
            (struct avg_order_accumulator){
                .sum = sum,
                .count = count,
            },
            key,
            sizeof(key)
        )) {
        sqlite3_result_error(context, "failed to format MyLite AVG order key", -1);
        return;
    }

    sqlite3_result_text(context, key, -1, SQLITE_TRANSIENT);
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
    double input = 0.0;
    int rc = MYLITE_OK;

    if (sqlite3_value_type(value) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    if (sqlite3_value_type(value) == SQLITE_INTEGER) {
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

    rc = numeric_double_value(context, value, &input);
    if (rc != MYLITE_OK) {
        numeric_sqlite_result_error_from_rc(context, rc, "failed to coerce ABS() argument");
        return;
    }
    numeric_function_finite_result_or_error(context, fabs(input), true);
}

static void numeric_sign_result(sqlite3_context *context, sqlite3_value *value) {
    double input = 0.0;
    int rc = MYLITE_OK;

    if (sqlite3_value_type(value) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    rc = numeric_double_value(context, value, &input);
    if (rc != MYLITE_OK) {
        numeric_sqlite_result_error_from_rc(context, rc, "failed to coerce SIGN() argument");
        return;
    }

    if (input > 0.0) {
        sqlite3_result_int64(context, 1);
    } else if (input < 0.0) {
        sqlite3_result_int64(context, -1);
    } else {
        sqlite3_result_int64(context, 0);
    }
}

static void numeric_round_result(sqlite3_context *context, int argc, sqlite3_value **argv) {
    double input = 0.0;
    int64_t places = 0;
    bool use_approximate_ties = false;
    int rc = MYLITE_OK;

    use_approximate_ties = numeric_round_value_uses_approximate_ties(argv[0]);
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        if (argc == 2 && sqlite3_value_type(argv[1]) != SQLITE_NULL) {
            rc = numeric_round_places_value(context, argv[1], &places);
            if (rc != MYLITE_OK) {
                numeric_sqlite_result_error_from_rc(
                    context,
                    rc,
                    "failed to coerce ROUND() places argument"
                );
                return;
            }
        }
        sqlite3_result_null(context);
        return;
    }
    rc = numeric_double_value(context, argv[0], &input);
    if (rc != MYLITE_OK) {
        numeric_sqlite_result_error_from_rc(context, rc, "failed to coerce ROUND() value argument");
        return;
    }
    if (argc == 2) {
        if (sqlite3_value_type(argv[1]) == SQLITE_NULL) {
            sqlite3_result_null(context);
            return;
        }
        rc = numeric_round_places_value(context, argv[1], &places);
        if (rc != MYLITE_OK) {
            numeric_sqlite_result_error_from_rc(
                context,
                rc,
                "failed to coerce ROUND() places argument"
            );
            return;
        }
    }
    numeric_function_finite_result_or_error(
        context,
        round_to_places((struct numeric_round_request){
            .value = input,
            .places = places,
            .use_approximate_ties = use_approximate_ties,
        }),
        true
    );
}

static void numeric_unary_result(
    sqlite3_context *context,
    enum mylite_numeric_function_operation operation,
    sqlite3_value *value
) {
    double input = 0.0;
    double pi = acos(-1.0);
    int rc = MYLITE_OK;

    if (sqlite3_value_type(value) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    if (operation == MYLITE_NUMERIC_OPERATION_BIT_COUNT) {
        uint64_t bit_count_input = 0U;

        rc = numeric_bit_count_value(context, value, &bit_count_input);
        if (rc != MYLITE_OK) {
            numeric_sqlite_result_error_from_rc(
                context,
                rc,
                "failed to coerce BIT_COUNT() argument"
            );
            return;
        }
        sqlite3_result_int64(context, bit_count_u64(bit_count_input));
        return;
    }

    rc = numeric_double_value(context, value, &input);
    if (rc != MYLITE_OK) {
        numeric_sqlite_result_error_from_rc(
            context,
            rc,
            "failed to coerce numeric function argument"
        );
        return;
    }

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
            rc = append_invalid_logarithm_warning(context);
            if (rc != MYLITE_OK) {
                numeric_sqlite_result_error_from_rc(context, rc, "failed to record LOG() warning");
                return;
            }
            sqlite3_result_null(context);
            return;
        }
        numeric_function_finite_result_or_error(context, log(input), false);
        return;
    case MYLITE_NUMERIC_OPERATION_LOG10:
        if (input <= 0.0) {
            rc = append_invalid_logarithm_warning(context);
            if (rc != MYLITE_OK) {
                numeric_sqlite_result_error_from_rc(
                    context,
                    rc,
                    "failed to record LOG10() warning"
                );
                return;
            }
            sqlite3_result_null(context);
            return;
        }
        numeric_function_finite_result_or_error(context, log10(input), false);
        return;
    case MYLITE_NUMERIC_OPERATION_LOG2:
        if (input <= 0.0) {
            rc = append_invalid_logarithm_warning(context);
            if (rc != MYLITE_OK) {
                numeric_sqlite_result_error_from_rc(context, rc, "failed to record LOG2() warning");
                return;
            }
            sqlite3_result_null(context);
            return;
        }
        numeric_function_finite_result_or_error(
            context,
            log(input) / log(logarithm_base_two),
            false
        );
        return;
    case MYLITE_NUMERIC_OPERATION_ABS:
    case MYLITE_NUMERIC_OPERATION_SIGN:
    case MYLITE_NUMERIC_OPERATION_ROUND:
    case MYLITE_NUMERIC_OPERATION_POW:
    case MYLITE_NUMERIC_OPERATION_BIT_COUNT:
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
    double first = 0.0;
    double second = 0.0;
    bool first_is_null = sqlite3_value_type(left) == SQLITE_NULL;
    bool second_is_null = sqlite3_value_type(right) == SQLITE_NULL;
    int rc = MYLITE_OK;

    if (!first_is_null) {
        rc = numeric_double_value(context, left, &first);
        if (rc != MYLITE_OK) {
            numeric_sqlite_result_error_from_rc(
                context,
                rc,
                "failed to coerce first numeric function argument"
            );
            return;
        }
    }
    if (!second_is_null) {
        rc = numeric_double_value(context, right, &second);
        if (rc != MYLITE_OK) {
            numeric_sqlite_result_error_from_rc(
                context,
                rc,
                "failed to coerce second numeric function argument"
            );
            return;
        }
    }

    switch (operation) {
    case MYLITE_NUMERIC_OPERATION_ATAN:
        if (first_is_null || second_is_null) {
            sqlite3_result_null(context);
            return;
        }
        numeric_function_finite_result_or_error(context, atan2(first, second), false);
        return;
    case MYLITE_NUMERIC_OPERATION_LOG:
        if ((!second_is_null && second <= 0.0) ||
            (!first_is_null && !second_is_null && (first <= 0.0 || first == 1.0))) {
            rc = append_invalid_logarithm_warning(context);
            if (rc != MYLITE_OK) {
                numeric_sqlite_result_error_from_rc(context, rc, "failed to record LOG() warning");
                return;
            }
        }
        if (first_is_null || second_is_null || first <= 0.0 || first == 1.0 || second <= 0.0) {
            sqlite3_result_null(context);
            return;
        }
        numeric_function_finite_result_or_error(context, log(second) / log(first), false);
        return;
    case MYLITE_NUMERIC_OPERATION_POW:
        if (first_is_null || second_is_null) {
            sqlite3_result_null(context);
            return;
        }
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

static int numeric_double_value(sqlite3_context *context, sqlite3_value *value, double *out_value) {
    const char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (context == NULL || value == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0.0;

    switch (sqlite3_value_type(value)) {
    case SQLITE_INTEGER:
        *out_value = (double)sqlite3_value_int64(value);
        return MYLITE_OK;
    case SQLITE_FLOAT:
        *out_value = sqlite3_value_double(value);
        return MYLITE_OK;
    case SQLITE_TEXT:
    case SQLITE_BLOB:
        rc = numeric_value_text(value, &text, &text_length);
        if (rc != MYLITE_OK) {
            return rc;
        }
        return numeric_text_double_value(context, text, text_length, out_value);
    case SQLITE_NULL:
        return MYLITE_OK;
    default:
        break;
    }

    return MYLITE_ERROR;
}

static int numeric_round_places_value(
    sqlite3_context *context,
    sqlite3_value *value,
    int64_t *out_places
) {
    const char *text = NULL;
    size_t text_length = 0U;
    struct numeric_integer_parse parse = {
        .signed_value = 0,
        .unsigned_value = 0U,
        .truncated = false,
    };
    double rounded = 0.0;
    int rc = MYLITE_OK;

    if (context == NULL || value == NULL || out_places == NULL) {
        return MYLITE_MISUSE;
    }
    *out_places = 0;

    switch (sqlite3_value_type(value)) {
    case SQLITE_INTEGER:
        *out_places = (int64_t)sqlite3_value_int64(value);
        return MYLITE_OK;
    case SQLITE_FLOAT:
        rounded = round(sqlite3_value_double(value));
        if (rounded > (double)INT64_MAX) {
            *out_places = INT64_MAX;
        } else if (rounded < (double)INT64_MIN) {
            *out_places = INT64_MIN;
        } else {
            *out_places = (int64_t)rounded;
        }
        return MYLITE_OK;
    case SQLITE_TEXT:
    case SQLITE_BLOB:
        rc = numeric_value_text(value, &text, &text_length);
        if (rc != MYLITE_OK) {
            return rc;
        }
        rc = numeric_text_integer_value(context, text, text_length, &parse);
        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_places = parse.signed_value;
        return MYLITE_OK;
    case SQLITE_NULL:
        return MYLITE_OK;
    default:
        break;
    }

    return MYLITE_ERROR;
}

static int numeric_bit_count_value(
    sqlite3_context *context,
    sqlite3_value *value,
    uint64_t *out_value
) {
    const char *text = NULL;
    size_t text_length = 0U;
    struct numeric_integer_parse parse = {
        .signed_value = 0,
        .unsigned_value = 0U,
        .truncated = false,
    };
    int rc = MYLITE_OK;

    if (context == NULL || value == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0U;

    switch (sqlite3_value_type(value)) {
    case SQLITE_INTEGER:
    case SQLITE_FLOAT:
        *out_value = (uint64_t)sqlite3_value_int64(value);
        return MYLITE_OK;
    case SQLITE_TEXT:
        rc = numeric_value_text(value, &text, &text_length);
        if (rc != MYLITE_OK) {
            return rc;
        }
        rc = numeric_text_integer_value(context, text, text_length, &parse);
        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_value = parse.unsigned_value;
        return MYLITE_OK;
    case SQLITE_BLOB:
        return MYLITE_ERROR;
    case SQLITE_NULL:
        return MYLITE_OK;
    default:
        break;
    }

    return MYLITE_ERROR;
}

static int numeric_text_double_value(
    sqlite3_context *context,
    const char *text,
    size_t text_length,
    double *out_value
) {
    char stack_buffer[numeric_double_parse_stack_capacity];
    char *buffer = stack_buffer;
    char *parse_end = NULL;
    bool saw_digits = false;
    bool truncated = false;
    size_t prefix_length = 0U;
    int rc = MYLITE_OK;

    if (context == NULL || (text == NULL && text_length != 0U) || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = 0.0;

    prefix_length = numeric_double_prefix_length(text, text_length, &saw_digits);
    truncated = !saw_digits || numeric_text_has_nonspace_suffix(text, text_length, prefix_length);
    if (!saw_digits) {
        return append_truncated_incorrect_number_warning(
            context,
            "DOUBLE",
            text == NULL ? "" : text,
            text_length
        );
    }

    if (prefix_length + 1U > sizeof(stack_buffer)) {
        buffer = (char *)malloc(prefix_length + 1U);
        if (buffer == NULL) {
            return MYLITE_NOMEM;
        }
    }
    memcpy(buffer, text, prefix_length);
    buffer[prefix_length] = '\0';

    errno = 0;
    *out_value = strtod(buffer, &parse_end);
    if (parse_end == buffer || errno == ERANGE) {
        truncated = true;
    }
    if (!isfinite(*out_value)) {
        *out_value = *out_value < 0.0 ? -DBL_MAX : DBL_MAX;
        truncated = true;
    }
    if (buffer != stack_buffer) {
        free(buffer);
    }

    if (truncated) {
        rc = append_truncated_incorrect_number_warning(
            context,
            "DOUBLE",
            text == NULL ? "" : text,
            text_length
        );
    }
    return rc;
}

static int numeric_text_integer_value(
    sqlite3_context *context,
    const char *text,
    size_t text_length,
    struct numeric_integer_parse *out_parse
) {
    size_t offset = 0U;
    bool is_negative = false;
    bool saw_digits = false;
    bool overflowed = false;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (context == NULL || (text == NULL && text_length != 0U) || out_parse == NULL) {
        return MYLITE_MISUSE;
    }
    *out_parse = (struct numeric_integer_parse){
        .signed_value = 0,
        .unsigned_value = 0U,
        .truncated = false,
    };

    while (offset < text_length && isspace((unsigned char)text[offset])) {
        ++offset;
    }
    if (offset < text_length && (text[offset] == '+' || text[offset] == '-')) {
        is_negative = text[offset] == '-';
        ++offset;
    }
    while (offset < text_length && isdigit((unsigned char)text[offset])) {
        unsigned int digit = (unsigned int)(text[offset] - '0');

        saw_digits = true;
        if (magnitude > (UINT64_MAX - digit) / integer_parse_base) {
            magnitude = UINT64_MAX;
            overflowed = true;
        } else if (!overflowed) {
            magnitude = magnitude * integer_parse_base + digit;
        }
        ++offset;
    }

    out_parse->truncated =
        !saw_digits || overflowed || numeric_text_has_nonspace_suffix(text, text_length, offset);
    if (!saw_digits) {
        magnitude = 0U;
    }
    if (is_negative) {
        out_parse->unsigned_value = 0U - magnitude;
        if (magnitude >= int64_min_magnitude) {
            out_parse->signed_value = INT64_MIN;
        } else {
            out_parse->signed_value = -(int64_t)magnitude;
        }
    } else {
        out_parse->unsigned_value = magnitude;
        out_parse->signed_value = magnitude > (uint64_t)INT64_MAX ? INT64_MAX : (int64_t)magnitude;
    }

    if (out_parse->truncated) {
        rc = append_truncated_incorrect_number_warning(
            context,
            "INTEGER",
            text == NULL ? "" : text,
            text_length
        );
    }
    return rc;
}

static int numeric_value_text(sqlite3_value *value, const char **out_text, size_t *out_length) {
    const unsigned char *text = NULL;
    int byte_count = 0;

    if (value == NULL || out_text == NULL || out_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_length = 0U;

    text = sqlite3_value_text(value);
    byte_count = sqlite3_value_bytes(value);
    if ((text == NULL && byte_count != 0) || byte_count < 0) {
        return MYLITE_NOMEM;
    }
    *out_text = (const char *)text;
    *out_length = (size_t)byte_count;
    return MYLITE_OK;
}

static size_t numeric_double_prefix_length(
    const char *text,
    size_t text_length,
    bool *out_saw_digits
) {
    size_t offset = 0U;
    size_t exponent_offset = 0U;
    bool saw_digits = false;
    bool exponent_has_digits = false;

    if (out_saw_digits == NULL) {
        return 0U;
    }
    *out_saw_digits = false;
    if (text == NULL) {
        return 0U;
    }

    while (offset < text_length && isspace((unsigned char)text[offset])) {
        ++offset;
    }
    if (offset < text_length && (text[offset] == '+' || text[offset] == '-')) {
        ++offset;
    }
    while (offset < text_length && isdigit((unsigned char)text[offset])) {
        saw_digits = true;
        ++offset;
    }
    if (offset < text_length && text[offset] == '.') {
        ++offset;
        while (offset < text_length && isdigit((unsigned char)text[offset])) {
            saw_digits = true;
            ++offset;
        }
    }
    if (!saw_digits) {
        return offset;
    }

    exponent_offset = offset;
    if (offset < text_length && (text[offset] == 'e' || text[offset] == 'E')) {
        ++offset;
        if (offset < text_length && (text[offset] == '+' || text[offset] == '-')) {
            ++offset;
        }
        while (offset < text_length && isdigit((unsigned char)text[offset])) {
            exponent_has_digits = true;
            ++offset;
        }
        if (!exponent_has_digits) {
            offset = exponent_offset;
        }
    }

    *out_saw_digits = true;
    return offset;
}

static bool numeric_text_has_nonspace_suffix(const char *text, size_t text_length, size_t offset) {
    if (text == NULL) {
        return false;
    }
    while (offset < text_length) {
        if (!isspace((unsigned char)text[offset])) {
            return true;
        }
        ++offset;
    }
    return false;
}

static int append_truncated_incorrect_number_warning(
    sqlite3_context *context,
    const char *type_name,
    const char *value,
    size_t value_length
) {
    struct mylite_db *database = NULL;
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = 0;
    int rc = MYLITE_OK;

    if (context == NULL || type_name == NULL || (value == NULL && value_length != 0U)) {
        return MYLITE_MISUSE;
    }

    database = mylite_sqlite_bootstrap_owner_from_context(context);
    if (database == NULL) {
        return MYLITE_ERROR;
    }

    written = snprintf(
        message,
        sizeof(message),
        "Truncated incorrect %s value: '%.*s'",
        type_name,
        value_length > numeric_warning_value_preview_length
            ? (int)numeric_warning_value_preview_length
            : (int)value_length,
        value == NULL ? "" : value
    );
    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        strcmp(type_name, "INTEGER") == 0 ? mysql_warning_truncated_incorrect_integer
                                          : mysql_warning_truncated_incorrect_double,
        "22007",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_NOMEM,
            "HY001",
            "out of memory while recording numeric function warning"
        );
    }
    return rc;
}

static int append_invalid_logarithm_warning(sqlite3_context *context) {
    struct mylite_db *database = NULL;
    int rc = MYLITE_OK;

    if (context == NULL) {
        return MYLITE_MISUSE;
    }
    database = mylite_sqlite_bootstrap_owner_from_context(context);
    if (database == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_invalid_argument_for_logarithm,
        "HY000",
        "Invalid argument for logarithm"
    );
    if (rc == MYLITE_NOMEM) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_NOMEM,
            "HY001",
            "out of memory while recording numeric function warning"
        );
    }
    return rc;
}

static void numeric_sqlite_result_error_from_rc(
    sqlite3_context *context,
    int rc,
    const char *message
) {
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    sqlite3_result_error(context, message == NULL ? "MyLite numeric function failed" : message, -1);
}

static bool numeric_round_value_uses_approximate_ties(sqlite3_value *value) {
    int value_type = SQLITE_NULL;

    if (value == NULL) {
        return false;
    }
    value_type = sqlite3_value_type(value);
    return value_type == SQLITE_FLOAT || value_type == SQLITE_TEXT || value_type == SQLITE_BLOB;
}

static double round_to_places(struct numeric_round_request request) {
    double scale = 0.0;

    if (request.places == 0) {
        return round_to_integral(request.value, request.use_approximate_ties);
    }
    if (request.places > round_scale_place_limit) {
        return request.value;
    }
    if (request.places < -round_scale_place_limit) {
        return 0.0;
    }

    if (request.places > 0) {
        scale = pow(logarithm_base_ten, (double)request.places);
        return round_to_integral(request.value * scale, request.use_approximate_ties) / scale;
    }

    scale = pow(logarithm_base_ten, (double)-request.places);
    return round_to_integral(request.value / scale, request.use_approximate_ties) * scale;
}

static double round_to_integral(double value, bool use_approximate_ties) {
    double lower = 0.0;
    double fraction = 0.0;

    if (!use_approximate_ties) {
        return round(value);
    }

    lower = floor(value);
    fraction = value - lower;
    if (fraction < approximate_round_tie_fraction) {
        return lower;
    }
    if (fraction > approximate_round_tie_fraction) {
        return lower + 1.0;
    }
    if (fmod(fabs(lower), integral_even_modulus) == 0.0) {
        return lower;
    }
    return lower + 1.0;
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

static bool format_avg_order_key(
    struct avg_order_accumulator accumulator,
    char *buffer,
    size_t buffer_size
) {
    uint64_t denominator = 0U;
    uint64_t magnitude = 0U;
    uint64_t integer_part = 0U;
    uint64_t remainder = 0U;
    uint64_t order_integer = 0U;
    bool is_negative = accumulator.sum < 0;
    int written = 0;

    if (buffer == NULL || buffer_size < avg_order_key_capacity || accumulator.count <= 0) {
        return false;
    }
    denominator = (uint64_t)accumulator.count;
    magnitude = avg_order_absolute_int64_magnitude(accumulator.sum);
    integer_part = magnitude / denominator;
    remainder = magnitude % denominator;
    order_integer = integer_part;
    if (is_negative) {
        if (integer_part > int64_min_magnitude) {
            return false;
        }
        order_integer = int64_min_magnitude - integer_part;
    }

    written = snprintf(
        buffer,
        buffer_size,
        "%c%0*" PRIu64 ".",
        is_negative ? '0' : '1',
        avg_order_integer_digits,
        order_integer
    );
    if (written != avg_order_fraction_offset) {
        return false;
    }

    for (size_t digit_index = 0U; digit_index < avg_order_fraction_digits; ++digit_index) {
        int digit = avg_order_next_decimal_digit(&remainder, denominator);

        if (digit < 0) {
            return false;
        }
        if (is_negative) {
            digit = (int)(integer_parse_base - 1U) - digit;
        }
        buffer[avg_order_fraction_offset + digit_index] = (char)('0' + digit);
    }
    buffer[avg_order_fraction_offset + avg_order_fraction_digits] = '\0';
    return true;
}

static bool format_avg_window_result(
    struct avg_order_accumulator accumulator,
    char *buffer,
    size_t buffer_size
) {
    uint64_t denominator = 0U;
    uint64_t magnitude = 0U;
    uint64_t integer_part = 0U;
    uint64_t remainder = 0U;
    unsigned int fraction = 0U;
    bool is_negative = accumulator.sum < 0;
    int round_digit = 0;
    int written = 0;

    if (buffer == NULL || buffer_size < avg_window_result_capacity || accumulator.count <= 0) {
        return false;
    }
    denominator = (uint64_t)accumulator.count;
    magnitude = avg_order_absolute_int64_magnitude(accumulator.sum);
    integer_part = magnitude / denominator;
    remainder = magnitude % denominator;

    for (size_t digit_index = 0U; digit_index < avg_window_result_fraction_digits; ++digit_index) {
        int digit = avg_order_next_decimal_digit(&remainder, denominator);

        if (digit < 0) {
            return false;
        }
        fraction = (fraction * avg_window_result_decimal_base) + (unsigned int)digit;
    }

    round_digit = avg_order_next_decimal_digit(&remainder, denominator);
    if (round_digit < 0) {
        return false;
    }
    if (round_digit >= avg_window_result_round_half_digit) {
        ++fraction;
        if (fraction == avg_window_result_fraction_scale) {
            fraction = 0U;
            ++integer_part;
        }
    }

    written = snprintf(
        buffer,
        buffer_size,
        "%s%" PRIu64 ".%04u",
        is_negative && (integer_part != 0U || fraction != 0U) ? "-" : "",
        integer_part,
        fraction
    );
    return written >= 0 && (size_t)written < buffer_size;
}

static uint64_t avg_order_absolute_int64_magnitude(int64_t value) {
    if (value >= 0) {
        return (uint64_t)value;
    }
    if (value == INT64_MIN) {
        return int64_min_magnitude;
    }

    return (uint64_t)-value;
}

static int avg_order_next_decimal_digit(uint64_t *remainder, uint64_t denominator) {
    struct avg_order_uint128_parts product = {0};
    int digit = 0;

    if (remainder == NULL || denominator == 0U || *remainder >= denominator) {
        return -1;
    }

    product = avg_order_multiply_u64_by_decimal_radix(*remainder);
    while (avg_order_uint128_ge_u64(&product, denominator)) {
        avg_order_uint128_subtract_u64(&product, denominator);
        ++digit;
    }

    *remainder = product.low;
    return digit;
}

static struct avg_order_uint128_parts avg_order_multiply_u64_by_decimal_radix(uint64_t value) {
    struct avg_order_uint128_parts product = {0};

    for (uint64_t index = 0U; index < integer_parse_base; ++index) {
        uint64_t previous_low = product.low;

        product.low += value;
        if (product.low < previous_low) {
            ++product.high;
        }
    }

    return product;
}

static bool avg_order_uint128_ge_u64(const struct avg_order_uint128_parts *left, uint64_t right) {
    if (left->high != 0U) {
        return true;
    }

    return left->low >= right;
}

static void avg_order_uint128_subtract_u64(struct avg_order_uint128_parts *left, uint64_t right) {
    uint64_t previous_low = left->low;

    left->low -= right;
    if (previous_low < right) {
        --left->high;
    }
}
