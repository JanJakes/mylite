#include "mylite_integer_arithmetic.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_mysql_error_codes.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum mylite_integer_arithmetic_operation {
    MYLITE_INTEGER_ARITHMETIC_ADD = 1,
    MYLITE_INTEGER_ARITHMETIC_SUBTRACT = 2,
    MYLITE_INTEGER_ARITHMETIC_MULTIPLY = 3,
    MYLITE_INTEGER_ARITHMETIC_DIVIDE = 4,
    MYLITE_INTEGER_ARITHMETIC_INTEGER_DIVIDE = 5,
    MYLITE_INTEGER_ARITHMETIC_MODULO = 6,
};

struct mylite_integer_arithmetic_operands {
    int64_t left;
    int64_t right;
};

struct mylite_arithmetic_number {
    double real;
};

enum {
    arithmetic_double_parse_stack_capacity = 128,
};

static const enum mylite_integer_arithmetic_operation add_operation = MYLITE_INTEGER_ARITHMETIC_ADD;
static const enum mylite_integer_arithmetic_operation subtract_operation =
    MYLITE_INTEGER_ARITHMETIC_SUBTRACT;
static const enum mylite_integer_arithmetic_operation multiply_operation =
    MYLITE_INTEGER_ARITHMETIC_MULTIPLY;
static const enum mylite_integer_arithmetic_operation divide_operation =
    MYLITE_INTEGER_ARITHMETIC_DIVIDE;
static const enum mylite_integer_arithmetic_operation integer_divide_operation =
    MYLITE_INTEGER_ARITHMETIC_INTEGER_DIVIDE;
static const enum mylite_integer_arithmetic_operation modulo_operation =
    MYLITE_INTEGER_ARITHMETIC_MODULO;

static const double int64_max_plus_one_as_double = 9223372036854775808.0;
static const double int64_min_as_double = -9223372036854775808.0;
static const size_t arithmetic_warning_value_preview_length = 200U;

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
static void checked_or_numeric_arithmetic_result(
    sqlite3_context *context,
    enum mylite_integer_arithmetic_operation operation,
    sqlite3_value *left_value,
    sqlite3_value *right_value
);
static int checked_integer_division_operation(
    sqlite3_context *context,
    enum mylite_integer_arithmetic_operation operation,
    struct mylite_integer_arithmetic_operands operands,
    int64_t *out_result,
    bool *out_is_null,
    bool *out_overflow
);
static int mylite_arithmetic_number_value(
    sqlite3_context *context,
    sqlite3_value *value,
    const char *warning_type,
    struct mylite_arithmetic_number *out_number
);
static int mylite_arithmetic_text_double_value(
    sqlite3_context *context,
    const char *text,
    size_t text_length,
    const char *warning_type,
    double *out_value
);
static int mylite_arithmetic_value_text(
    sqlite3_value *value,
    const char **out_text,
    size_t *out_length
);
static size_t mylite_arithmetic_double_prefix_length(
    const char *text,
    size_t text_length,
    bool *out_saw_digits
);
static bool mylite_arithmetic_text_has_nonspace_suffix(
    const char *text,
    size_t text_length,
    size_t offset
);
static int append_truncated_incorrect_arithmetic_warning(
    sqlite3_context *context,
    const char *type_name,
    const char *value,
    size_t value_length
);
static int append_arithmetic_division_by_zero_warning(sqlite3_context *context);
static bool mylite_arithmetic_operation_is_valid(enum mylite_integer_arithmetic_operation operation
);
static bool mylite_arithmetic_operation_uses_decimal_text_warning(
    enum mylite_integer_arithmetic_operation operation
);
static void mylite_arithmetic_numeric_result(
    sqlite3_context *context,
    double value,
    bool use_integer_when_exact
);
static bool mylite_arithmetic_double_to_int64(double value, int64_t *out_result);
static bool mylite_arithmetic_double_is_integral(double value);
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
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_arithmetic_add",
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
            .name = "_mylite_arithmetic_sub",
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
            .name = "_mylite_arithmetic_mul",
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
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_arithmetic_div",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&divide_operation,
            .scalar_callback = integer_arithmetic_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_arithmetic_int_div",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&integer_divide_operation,
            .scalar_callback = integer_arithmetic_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_arithmetic_mod",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&modulo_operation,
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

    if (context == NULL || argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL) {
        sqlite3_result_error(context, "invalid MyLite arithmetic callback", -1);
        return;
    }
    if (!integer_arithmetic_operation_from_context(context, &operation)) {
        sqlite3_result_error(context, "invalid MyLite arithmetic operation", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    checked_or_numeric_arithmetic_result(context, operation, argv[0], argv[1]);
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
    if (operation == NULL || !mylite_arithmetic_operation_is_valid(*operation)) {
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
    case MYLITE_INTEGER_ARITHMETIC_DIVIDE:
    case MYLITE_INTEGER_ARITHMETIC_INTEGER_DIVIDE:
    case MYLITE_INTEGER_ARITHMETIC_MODULO:
        break;
    }

    return true;
}

static void checked_or_numeric_arithmetic_result(
    sqlite3_context *context,
    enum mylite_integer_arithmetic_operation operation,
    sqlite3_value *left_value,
    sqlite3_value *right_value
) {
    struct mylite_arithmetic_number left_number = {0};
    struct mylite_arithmetic_number right_number = {0};
    int64_t integer_result = 0;
    bool is_null_result = false;
    bool overflow = false;
    int rc = MYLITE_OK;
    const char *warning_type =
        mylite_arithmetic_operation_uses_decimal_text_warning(operation) ? "DECIMAL" : "DOUBLE";

    if (sqlite3_value_type(left_value) == SQLITE_INTEGER &&
        sqlite3_value_type(right_value) == SQLITE_INTEGER) {
        struct mylite_integer_arithmetic_operands operands = {
            .left = (int64_t)sqlite3_value_int64(left_value),
            .right = (int64_t)sqlite3_value_int64(right_value),
        };

        if (operation == MYLITE_INTEGER_ARITHMETIC_ADD ||
            operation == MYLITE_INTEGER_ARITHMETIC_SUBTRACT ||
            operation == MYLITE_INTEGER_ARITHMETIC_MULTIPLY) {
            if (checked_integer_arithmetic_operation(operation, operands, &integer_result)) {
                sqlite3_result_error(context, MYLITE_INTEGER_ARITHMETIC_OVERFLOW_MESSAGE, -1);
                return;
            }
            sqlite3_result_int64(context, (sqlite3_int64)integer_result);
            return;
        }
        rc = checked_integer_division_operation(
            context,
            operation,
            operands,
            &integer_result,
            &is_null_result,
            &overflow
        );
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
            return;
        }
        if (rc != MYLITE_OK) {
            sqlite3_result_error(context, "failed to record MyLite division warning", -1);
            return;
        }
        if (overflow) {
            sqlite3_result_error(context, MYLITE_INTEGER_ARITHMETIC_OVERFLOW_MESSAGE, -1);
            return;
        }
        if (is_null_result) {
            sqlite3_result_null(context);
            return;
        }
        if (operation == MYLITE_INTEGER_ARITHMETIC_DIVIDE) {
            mylite_arithmetic_numeric_result(
                context,
                (double)operands.left / (double)operands.right,
                false
            );
            return;
        }
        sqlite3_result_int64(context, (sqlite3_int64)integer_result);
        return;
    }

    rc = mylite_arithmetic_number_value(context, left_value, warning_type, &left_number);
    if (rc == MYLITE_OK) {
        rc = mylite_arithmetic_number_value(context, right_value, warning_type, &right_number);
    }
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
        } else {
            sqlite3_result_error(context, "failed to coerce MyLite arithmetic argument", -1);
        }
        return;
    }

    switch (operation) {
    case MYLITE_INTEGER_ARITHMETIC_ADD:
        mylite_arithmetic_numeric_result(context, left_number.real + right_number.real, true);
        return;
    case MYLITE_INTEGER_ARITHMETIC_SUBTRACT:
        mylite_arithmetic_numeric_result(context, left_number.real - right_number.real, true);
        return;
    case MYLITE_INTEGER_ARITHMETIC_MULTIPLY:
        mylite_arithmetic_numeric_result(context, left_number.real * right_number.real, true);
        return;
    case MYLITE_INTEGER_ARITHMETIC_DIVIDE:
        if (right_number.real == 0.0) {
            rc = append_arithmetic_division_by_zero_warning(context);
            if (rc == MYLITE_NOMEM) {
                sqlite3_result_error_nomem(context);
            } else if (rc != MYLITE_OK) {
                sqlite3_result_error(context, "failed to record MyLite division warning", -1);
            } else {
                sqlite3_result_null(context);
            }
            return;
        }
        mylite_arithmetic_numeric_result(context, left_number.real / right_number.real, false);
        return;
    case MYLITE_INTEGER_ARITHMETIC_INTEGER_DIVIDE:
        if (right_number.real == 0.0) {
            rc = append_arithmetic_division_by_zero_warning(context);
            if (rc == MYLITE_NOMEM) {
                sqlite3_result_error_nomem(context);
            } else if (rc != MYLITE_OK) {
                sqlite3_result_error(context, "failed to record MyLite division warning", -1);
            } else {
                sqlite3_result_null(context);
            }
            return;
        }
        if (!mylite_arithmetic_double_to_int64(
                left_number.real / right_number.real,
                &integer_result
            )) {
            sqlite3_result_error(context, MYLITE_INTEGER_ARITHMETIC_OVERFLOW_MESSAGE, -1);
            return;
        }
        sqlite3_result_int64(context, (sqlite3_int64)integer_result);
        return;
    case MYLITE_INTEGER_ARITHMETIC_MODULO:
        if (right_number.real == 0.0) {
            rc = append_arithmetic_division_by_zero_warning(context);
            if (rc == MYLITE_NOMEM) {
                sqlite3_result_error_nomem(context);
            } else if (rc != MYLITE_OK) {
                sqlite3_result_error(context, "failed to record MyLite division warning", -1);
            } else {
                sqlite3_result_null(context);
            }
            return;
        }
        mylite_arithmetic_numeric_result(context, fmod(left_number.real, right_number.real), true);
        return;
    }

    sqlite3_result_error(context, "unsupported MyLite arithmetic operation", -1);
}

static int checked_integer_division_operation(
    sqlite3_context *context,
    enum mylite_integer_arithmetic_operation operation,
    struct mylite_integer_arithmetic_operands operands,
    int64_t *out_result,
    bool *out_is_null,
    bool *out_overflow
) {
    int rc = MYLITE_OK;

    if (out_result == NULL || out_is_null == NULL || out_overflow == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = 0;
    *out_is_null = false;
    *out_overflow = false;

    if (operands.right == 0) {
        rc = append_arithmetic_division_by_zero_warning(context);
        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_is_null = true;
        return MYLITE_OK;
    }
    switch (operation) {
    case MYLITE_INTEGER_ARITHMETIC_DIVIDE:
        return MYLITE_OK;
    case MYLITE_INTEGER_ARITHMETIC_INTEGER_DIVIDE:
        if (operands.left == INT64_MIN && operands.right == -1) {
            *out_overflow = true;
            return MYLITE_OK;
        }
        *out_result = operands.left / operands.right;
        return MYLITE_OK;
    case MYLITE_INTEGER_ARITHMETIC_MODULO:
        if (operands.right == -1) {
            *out_result = 0;
        } else {
            *out_result = operands.left % operands.right;
        }
        return MYLITE_OK;
    case MYLITE_INTEGER_ARITHMETIC_ADD:
    case MYLITE_INTEGER_ARITHMETIC_SUBTRACT:
    case MYLITE_INTEGER_ARITHMETIC_MULTIPLY:
        break;
    }
    return MYLITE_ERROR;
}

static int mylite_arithmetic_number_value(
    sqlite3_context *context,
    sqlite3_value *value,
    const char *warning_type,
    struct mylite_arithmetic_number *out_number
) {
    const char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (value == NULL || out_number == NULL) {
        return MYLITE_MISUSE;
    }
    *out_number = (struct mylite_arithmetic_number){0};

    switch (sqlite3_value_type(value)) {
    case SQLITE_INTEGER:
        out_number->real = (double)sqlite3_value_int64(value);
        return MYLITE_OK;
    case SQLITE_FLOAT:
        out_number->real = sqlite3_value_double(value);
        return MYLITE_OK;
    case SQLITE_TEXT:
        rc = mylite_arithmetic_value_text(value, &text, &text_length);
        if (rc != MYLITE_OK) {
            return rc;
        }
        return mylite_arithmetic_text_double_value(
            context,
            text,
            text_length,
            warning_type,
            &out_number->real
        );
    case SQLITE_BLOB:
        return MYLITE_ERROR;
    case SQLITE_NULL:
        return MYLITE_OK;
    default:
        break;
    }
    return MYLITE_ERROR;
}

static int mylite_arithmetic_text_double_value(
    sqlite3_context *context,
    const char *text,
    size_t text_length,
    const char *warning_type,
    double *out_value
) {
    char stack_buffer[arithmetic_double_parse_stack_capacity];
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

    prefix_length = mylite_arithmetic_double_prefix_length(text, text_length, &saw_digits);
    truncated =
        !saw_digits || mylite_arithmetic_text_has_nonspace_suffix(text, text_length, prefix_length);
    if (!saw_digits) {
        return append_truncated_incorrect_arithmetic_warning(
            context,
            warning_type,
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
        rc = append_truncated_incorrect_arithmetic_warning(
            context,
            warning_type,
            text == NULL ? "" : text,
            text_length
        );
    }
    return rc;
}

static int mylite_arithmetic_value_text(
    sqlite3_value *value,
    const char **out_text,
    size_t *out_length
) {
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

static size_t mylite_arithmetic_double_prefix_length(
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

static bool mylite_arithmetic_text_has_nonspace_suffix(
    const char *text,
    size_t text_length,
    size_t offset
) {
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

static int append_truncated_incorrect_arithmetic_warning(
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
        value_length > arithmetic_warning_value_preview_length
            ? (int)arithmetic_warning_value_preview_length
            : (int)value_length,
        value == NULL ? "" : value
    );
    if (written < 0) {
        message[0] = '\0';
    }
    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_truncated_incorrect_double,
        "22007",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_NOMEM,
            "HY001",
            "out of memory while recording arithmetic warning"
        );
    }
    return rc;
}

static int append_arithmetic_division_by_zero_warning(sqlite3_context *context) {
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
        mysql_warning_division_by_zero,
        "22012",
        "Division by 0"
    );
    if (rc == MYLITE_NOMEM) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_NOMEM,
            "HY001",
            "out of memory while recording arithmetic warning"
        );
    }
    return rc;
}

static bool mylite_arithmetic_operation_is_valid(enum mylite_integer_arithmetic_operation operation
) {
    switch (operation) {
    case MYLITE_INTEGER_ARITHMETIC_ADD:
    case MYLITE_INTEGER_ARITHMETIC_SUBTRACT:
    case MYLITE_INTEGER_ARITHMETIC_MULTIPLY:
    case MYLITE_INTEGER_ARITHMETIC_DIVIDE:
    case MYLITE_INTEGER_ARITHMETIC_INTEGER_DIVIDE:
    case MYLITE_INTEGER_ARITHMETIC_MODULO:
        return true;
    }
    return false;
}

static bool mylite_arithmetic_operation_uses_decimal_text_warning(
    enum mylite_integer_arithmetic_operation operation
) {
    return operation == MYLITE_INTEGER_ARITHMETIC_INTEGER_DIVIDE;
}

static void mylite_arithmetic_numeric_result(
    sqlite3_context *context,
    double value,
    bool use_integer_when_exact
) {
    int64_t integer = 0;

    if (!isfinite(value)) {
        sqlite3_result_error(context, "DOUBLE value is out of range", -1);
        return;
    }
    if (value == 0.0) {
        value = 0.0;
    }
    if (use_integer_when_exact && mylite_arithmetic_double_to_int64(value, &integer) &&
        mylite_arithmetic_double_is_integral(value)) {
        sqlite3_result_int64(context, (sqlite3_int64)integer);
        return;
    }
    sqlite3_result_double(context, value);
}

static bool mylite_arithmetic_double_to_int64(double value, int64_t *out_result) {
    double truncated = 0.0;

    if (out_result == NULL || !isfinite(value) || value >= int64_max_plus_one_as_double ||
        value < int64_min_as_double) {
        return false;
    }
    truncated = value < 0.0 ? ceil(value) : floor(value);
    *out_result = (int64_t)truncated;
    return true;
}

static bool mylite_arithmetic_double_is_integral(double value) {
    double integral = 0.0;

    if (!isfinite(value)) {
        return false;
    }
    integral = value < 0.0 ? ceil(value) : floor(value);
    return integral == value;
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
