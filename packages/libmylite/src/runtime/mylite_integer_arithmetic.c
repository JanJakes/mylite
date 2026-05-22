#include "mylite_integer_arithmetic.h"

#include "mylite_sqlite_registration.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

enum mylite_integer_arithmetic_operation {
    MYLITE_INTEGER_ARITHMETIC_ADD = 1,
    MYLITE_INTEGER_ARITHMETIC_SUBTRACT = 2,
    MYLITE_INTEGER_ARITHMETIC_MULTIPLY = 3,
};

struct mylite_integer_arithmetic_operands {
    int64_t left;
    int64_t right;
};

static const enum mylite_integer_arithmetic_operation add_operation = MYLITE_INTEGER_ARITHMETIC_ADD;
static const enum mylite_integer_arithmetic_operation subtract_operation =
    MYLITE_INTEGER_ARITHMETIC_SUBTRACT;
static const enum mylite_integer_arithmetic_operation multiply_operation =
    MYLITE_INTEGER_ARITHMETIC_MULTIPLY;

static void integer_arithmetic_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static bool integer_arithmetic_operation_from_context(
    sqlite3_context *context,
    enum mylite_integer_arithmetic_operation *out_operation
);
static bool checked_integer_arithmetic_operation(
    enum mylite_integer_arithmetic_operation operation,
    struct mylite_integer_arithmetic_operands operands,
    int64_t *out_result
);
static bool checked_int64_add(int64_t left, int64_t right, int64_t *out_result);
static bool checked_int64_subtract(int64_t left, int64_t right, int64_t *out_result);
static bool checked_int64_multiply(int64_t left, int64_t right, int64_t *out_result);

int mylite_sqlite_register_integer_arithmetic_functions(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_i64_add",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&add_operation,
            .scalar_callback = integer_arithmetic_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_i64_sub",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&subtract_operation,
            .scalar_callback = integer_arithmetic_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_i64_mul",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&multiply_operation,
            .scalar_callback = integer_arithmetic_sqlite_callback,
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

static void integer_arithmetic_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    enum mylite_integer_arithmetic_operation operation = MYLITE_INTEGER_ARITHMETIC_ADD;
    int64_t left = 0;
    int64_t right = 0;
    int64_t result = 0;

    if (context == NULL || argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL) {
        sqlite3_result_error(context, "invalid MyLite signed integer arithmetic callback", -1);
        return;
    }
    if (!integer_arithmetic_operation_from_context(context, &operation)) {
        sqlite3_result_error(context, "invalid MyLite signed integer arithmetic operation", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    if (sqlite3_value_type(argv[0]) != SQLITE_INTEGER ||
        sqlite3_value_type(argv[1]) != SQLITE_INTEGER) {
        sqlite3_result_error(context, "invalid MyLite signed integer arithmetic argument", -1);
        return;
    }

    left = (int64_t)sqlite3_value_int64(argv[0]);
    right = (int64_t)sqlite3_value_int64(argv[1]);
    if (checked_integer_arithmetic_operation(
            operation,
            (struct mylite_integer_arithmetic_operands){.left = left, .right = right},
            &result
        )) {
        sqlite3_result_error(context, MYLITE_INTEGER_ARITHMETIC_OVERFLOW_MESSAGE, -1);
        return;
    }

    sqlite3_result_int64(context, (sqlite3_int64)result);
}

static bool integer_arithmetic_operation_from_context(
    sqlite3_context *context,
    enum mylite_integer_arithmetic_operation *out_operation
) {
    const enum mylite_integer_arithmetic_operation *operation = NULL;

    if (context == NULL || out_operation == NULL) {
        return false;
    }

    operation = (const enum mylite_integer_arithmetic_operation *)sqlite3_user_data(context);
    if (operation == NULL || (*operation != MYLITE_INTEGER_ARITHMETIC_ADD &&
                              *operation != MYLITE_INTEGER_ARITHMETIC_SUBTRACT &&
                              *operation != MYLITE_INTEGER_ARITHMETIC_MULTIPLY)) {
        return false;
    }

    *out_operation = *operation;
    return true;
}

static bool checked_integer_arithmetic_operation(
    enum mylite_integer_arithmetic_operation operation,
    struct mylite_integer_arithmetic_operands operands,
    int64_t *out_result
) {
    switch (operation) {
    case MYLITE_INTEGER_ARITHMETIC_ADD:
        return checked_int64_add(operands.left, operands.right, out_result);
    case MYLITE_INTEGER_ARITHMETIC_SUBTRACT:
        return checked_int64_subtract(operands.left, operands.right, out_result);
    case MYLITE_INTEGER_ARITHMETIC_MULTIPLY:
        return checked_int64_multiply(operands.left, operands.right, out_result);
    }

    return true;
}

static bool checked_int64_add(int64_t left, int64_t right, int64_t *out_result) {
    if ((right > 0 && left > INT64_MAX - right) || (right < 0 && left < INT64_MIN - right)) {
        return true;
    }

    *out_result = left + right;
    return false;
}

static bool checked_int64_subtract(int64_t left, int64_t right, int64_t *out_result) {
    if ((right < 0 && left > INT64_MAX + right) || (right > 0 && left < INT64_MIN + right)) {
        return true;
    }

    *out_result = left - right;
    return false;
}

static bool checked_int64_multiply(int64_t left, int64_t right, int64_t *out_result) {
    if (left == 0 || right == 0) {
        *out_result = 0;
        return false;
    }
    if ((left == INT64_MIN && right == -1) || (right == INT64_MIN && left == -1)) {
        return true;
    }
    if (left > 0) {
        if (right > 0) {
            if (left > INT64_MAX / right) {
                return true;
            }
        } else if (right < INT64_MIN / left) {
            return true;
        }
    } else if (right > 0) {
        if (left < INT64_MIN / right) {
            return true;
        }
    } else if (left < INT64_MAX / right) {
        return true;
    }

    *out_result = left * right;
    return false;
}
