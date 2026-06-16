#include "mylite_base_conversion.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_execution_scalar_binary_internal.h"
#include "mylite_mysql_error_codes.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <mylite/mylite.h>

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

enum mylite_base_conversion_operation {
    MYLITE_BASE_CONVERSION_OPERATION_NONE = 0,
    MYLITE_BASE_CONVERSION_OPERATION_BIN = 1,
    MYLITE_BASE_CONVERSION_OPERATION_OCT = 2,
    MYLITE_BASE_CONVERSION_OPERATION_CONV = 3,
};

enum {
    base_conversion_sql_input_text_capacity = binary_integer_text_capacity,
};

struct row_conv_digit_parse {
    uint64_t value;
    bool saw_digit;
    bool overflowed;
};

struct row_conv_input_conversion {
    int64_t value;
    unsigned int from_base;
    bool signed_input;
};

struct row_conv_output_conversion {
    uint64_t value;
    unsigned int base;
    bool signed_output;
};

static void base_conversion_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static bool base_conversion_operation_from_context(
    sqlite3_context *context,
    enum mylite_base_conversion_operation *out_operation
);
static void bin_oct_result(sqlite3_context *context, sqlite3_value *value, unsigned int base);
static void conv_result(sqlite3_context *context, sqlite3_value **argv);
static bool sqlite_value_int64_or_null(
    sqlite3_context *context,
    sqlite3_value *value,
    int64_t *out_value,
    bool *out_is_null
);
static unsigned int absolute_conv_base(int64_t base);
static int convert_conv_integer_value(
    struct mylite_db *database,
    struct row_conv_input_conversion input,
    uint64_t *out_value
);
static int parse_row_conv_input_digits(
    const char *input_text,
    unsigned int from_base,
    uint64_t limit,
    struct row_conv_digit_parse *out_parse
);
static int format_row_conv_input_text(
    struct mylite_db *database,
    int64_t value,
    char *buffer,
    size_t buffer_size
);
static int append_row_conv_truncated_decimal_warning(
    struct mylite_db *database,
    const char *input_text
);
static int format_row_conv_output_value(
    struct mylite_db *database,
    struct row_conv_output_conversion output,
    char *buffer,
    size_t buffer_size
);
static void result_error_from_mylite_status(sqlite3_context *context, int status);

int mylite_sqlite_register_base_conversion_functions(sqlite3 *sqlite) {
    static const enum mylite_base_conversion_operation operations[] = {
        MYLITE_BASE_CONVERSION_OPERATION_NONE,
        MYLITE_BASE_CONVERSION_OPERATION_BIN,
        MYLITE_BASE_CONVERSION_OPERATION_OCT,
        MYLITE_BASE_CONVERSION_OPERATION_CONV,
    };
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_bin",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&operations[MYLITE_BASE_CONVERSION_OPERATION_BIN],
            .scalar_callback = base_conversion_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_oct",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&operations[MYLITE_BASE_CONVERSION_OPERATION_OCT],
            .scalar_callback = base_conversion_sqlite_callback,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_conv",
            .argument_count = 3,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&operations[MYLITE_BASE_CONVERSION_OPERATION_CONV],
            .scalar_callback = base_conversion_sqlite_callback,
        },
    };

    return mylite_sqlite_register_functions(
        sqlite,
        registrations,
        sizeof(registrations) / sizeof(registrations[0])
    );
}

static void base_conversion_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    enum mylite_base_conversion_operation operation = MYLITE_BASE_CONVERSION_OPERATION_NONE;

    if (context == NULL || argc < 1 || argc > 3 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite base conversion callback", -1);
        return;
    }
    if (!base_conversion_operation_from_context(context, &operation)) {
        sqlite3_result_error(context, "invalid MyLite base conversion operation", -1);
        return;
    }

    switch (operation) {
    case MYLITE_BASE_CONVERSION_OPERATION_BIN:
        if (argc != 1) {
            sqlite3_result_error(context, "invalid MyLite BIN() callback arity", -1);
            return;
        }
        bin_oct_result(context, argv[0], binary_base_conversion_binary_base);
        return;
    case MYLITE_BASE_CONVERSION_OPERATION_OCT:
        if (argc != 1) {
            sqlite3_result_error(context, "invalid MyLite OCT() callback arity", -1);
            return;
        }
        bin_oct_result(context, argv[0], binary_base_conversion_octal_base);
        return;
    case MYLITE_BASE_CONVERSION_OPERATION_CONV:
        if (argc != 3 || argv[1] == NULL || argv[2] == NULL) {
            sqlite3_result_error(context, "invalid MyLite CONV() callback arity", -1);
            return;
        }
        conv_result(context, argv);
        return;
    case MYLITE_BASE_CONVERSION_OPERATION_NONE:
        break;
    }

    sqlite3_result_error(context, "invalid MyLite base conversion operation", -1);
}

static bool base_conversion_operation_from_context(
    sqlite3_context *context,
    enum mylite_base_conversion_operation *out_operation
) {
    const enum mylite_base_conversion_operation *operation = NULL;

    if (context == NULL || out_operation == NULL) {
        return false;
    }
    operation = (const enum mylite_base_conversion_operation *)sqlite3_user_data(context);
    if (operation == NULL || *operation <= MYLITE_BASE_CONVERSION_OPERATION_NONE ||
        *operation > MYLITE_BASE_CONVERSION_OPERATION_CONV) {
        return false;
    }
    *out_operation = *operation;
    return true;
}

static void bin_oct_result(sqlite3_context *context, sqlite3_value *value, unsigned int base) {
    struct mylite_db *database = NULL;
    char text[binary_base_conversion_text_capacity];
    int64_t integer = 0;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (!sqlite_value_int64_or_null(context, value, &integer, &is_null)) {
        return;
    }
    if (is_null) {
        sqlite3_result_null(context);
        return;
    }

    database = mylite_sqlite_bootstrap_owner_from_context(context);
    rc = mylite_execution_scalar_binary_format_base_conversion_value(
        database,
        (uint64_t)integer,
        base,
        text,
        sizeof(text)
    );
    if (rc != MYLITE_OK) {
        result_error_from_mylite_status(context, rc);
        return;
    }
    sqlite3_result_text(context, text, -1, SQLITE_TRANSIENT);
}

static void conv_result(sqlite3_context *context, sqlite3_value **argv) {
    struct mylite_db *database = NULL;
    char text[binary_base_conversion_text_capacity];
    uint64_t converted = 0U;
    int64_t value = 0;
    int64_t from_base_value = 0;
    int64_t to_base_value = 0;
    unsigned int from_base = 0U;
    unsigned int to_base = 0U;
    bool value_is_null = false;
    bool from_base_is_null = false;
    bool to_base_is_null = false;
    int rc = MYLITE_OK;

    if (!sqlite_value_int64_or_null(context, argv[0], &value, &value_is_null) ||
        !sqlite_value_int64_or_null(context, argv[1], &from_base_value, &from_base_is_null) ||
        !sqlite_value_int64_or_null(context, argv[2], &to_base_value, &to_base_is_null)) {
        return;
    }
    if (value_is_null || from_base_is_null || to_base_is_null) {
        sqlite3_result_null(context);
        return;
    }

    from_base = absolute_conv_base(from_base_value);
    to_base = absolute_conv_base(to_base_value);
    if (from_base < binary_base_conversion_binary_base ||
        from_base > binary_base_conversion_max_base ||
        to_base < binary_base_conversion_binary_base || to_base > binary_base_conversion_max_base) {
        sqlite3_result_null(context);
        return;
    }

    database = mylite_sqlite_bootstrap_owner_from_context(context);
    rc = convert_conv_integer_value(
        database,
        (struct row_conv_input_conversion){
            .value = value,
            .from_base = from_base,
            .signed_input = from_base_value < 0,
        },
        &converted
    );
    if (rc == MYLITE_OK) {
        rc = format_row_conv_output_value(
            database,
            (struct row_conv_output_conversion){
                .value = converted,
                .base = to_base,
                .signed_output = to_base_value < 0,
            },
            text,
            sizeof(text)
        );
    }
    if (rc != MYLITE_OK) {
        result_error_from_mylite_status(context, rc);
        return;
    }
    sqlite3_result_text(context, text, -1, SQLITE_TRANSIENT);
}

static bool sqlite_value_int64_or_null(
    sqlite3_context *context,
    sqlite3_value *value,
    int64_t *out_value,
    bool *out_is_null
) {
    int value_type = SQLITE_NULL;

    if (context == NULL || value == NULL || out_value == NULL || out_is_null == NULL) {
        sqlite3_result_error(context, "invalid MyLite base conversion argument", -1);
        return false;
    }

    value_type = sqlite3_value_type(value);
    if (value_type == SQLITE_NULL) {
        *out_value = 0;
        *out_is_null = true;
        return true;
    }
    if (value_type != SQLITE_INTEGER) {
        sqlite3_result_error(context, "unsupported MyLite base conversion argument type", -1);
        return false;
    }

    *out_value = sqlite3_value_int64(value);
    *out_is_null = false;
    return true;
}

static unsigned int absolute_conv_base(int64_t base) {
    if (base == INT64_MIN) {
        return binary_base_conversion_max_base + 1U;
    }
    if (base < 0) {
        return (unsigned int)(-base);
    }
    return (unsigned int)base;
}

static int convert_conv_integer_value(
    struct mylite_db *database,
    struct row_conv_input_conversion input,
    uint64_t *out_value
) {
    uint64_t limit = UINT64_MAX;
    struct row_conv_digit_parse parse = {0};
    char input_text[base_conversion_sql_input_text_capacity];
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (input.signed_input) {
        limit = (uint64_t)INT64_MAX;
        if (input.value < 0) {
            limit = (uint64_t)INT64_MAX + 1U;
        }
    }

    rc = format_row_conv_input_text(database, input.value, input_text, sizeof(input_text));
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = parse_row_conv_input_digits(input_text, input.from_base, limit, &parse);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!parse.saw_digit || parse.overflowed) {
        rc = append_row_conv_truncated_decimal_warning(database, input_text);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    if (!parse.saw_digit) {
        *out_value = 0U;
    } else if (input.value < 0) {
        *out_value = 0U - parse.value;
    } else {
        *out_value = parse.value;
    }
    return MYLITE_OK;
}

static int parse_row_conv_input_digits(
    const char *input_text,
    unsigned int from_base,
    uint64_t limit,
    struct row_conv_digit_parse *out_parse
) {
    struct row_conv_digit_parse parse = {0};
    size_t offset = 0U;

    if (input_text == NULL || out_parse == NULL) {
        return MYLITE_MISUSE;
    }
    if (input_text[offset] == '-') {
        ++offset;
    }
    while (input_text[offset] != '\0') {
        unsigned char byte = (unsigned char)input_text[offset];
        uint64_t digit = 0U;

        if (byte < '0' || byte > '9') {
            break;
        }
        digit = (uint64_t)(byte - '0');
        if (digit >= (uint64_t)from_base) {
            break;
        }
        parse.saw_digit = true;
        if (!parse.overflowed) {
            if (parse.value > (limit - digit) / (uint64_t)from_base) {
                parse.value = limit;
                parse.overflowed = true;
            } else {
                parse.value = (parse.value * (uint64_t)from_base) + digit;
            }
        }
        ++offset;
    }
    *out_parse = parse;
    return MYLITE_OK;
}

static int format_row_conv_input_text(
    struct mylite_db *database,
    int64_t value,
    char *buffer,
    size_t buffer_size
) {
    uint64_t magnitude = 0U;
    int written = 0;

    if (buffer == NULL || buffer_size == 0U) {
        return MYLITE_MISUSE;
    }
    if (value >= 0) {
        return mylite_execution_format_uint64(database, (uint64_t)value, buffer, buffer_size);
    }

    magnitude = value == INT64_MIN ? (uint64_t)INT64_MAX + 1U : (uint64_t)(-value);
    written = snprintf(buffer, buffer_size, "-%" PRIu64, magnitude);
    if (written < 0 || (size_t)written >= buffer_size) {
        mylite_execution_set_runtime_error(database, "failed to format CONV() input value");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_row_conv_truncated_decimal_warning(
    struct mylite_db *database,
    const char *input_text
) {
    char message
        [sizeof("Truncated incorrect DECIMAL value: ''") + base_conversion_sql_input_text_capacity];
    int written = 0;

    if (database == NULL || input_text == NULL) {
        return MYLITE_MISUSE;
    }

    written =
        snprintf(message, sizeof(message), "Truncated incorrect DECIMAL value: '%s'", input_text);
    if (written < 0 || (size_t)written >= sizeof(message)) {
        mylite_execution_set_runtime_error(database, "failed to format CONV() warning");
        return MYLITE_ERROR;
    }
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_truncated_incorrect_decimal,
        "22007",
        message
    );
}

static int format_row_conv_output_value(
    struct mylite_db *database,
    struct row_conv_output_conversion output,
    char *buffer,
    size_t buffer_size
) {
    uint64_t magnitude = output.value;
    bool is_negative = false;

    if (buffer == NULL || buffer_size == 0U || output.base < binary_base_conversion_binary_base ||
        output.base > binary_base_conversion_max_base) {
        mylite_execution_set_runtime_error(database, "failed to format CONV() value");
        return MYLITE_ERROR;
    }
    if (output.signed_output && output.value > (uint64_t)INT64_MAX) {
        is_negative = true;
        magnitude = 0U - output.value;
    }
    if (is_negative) {
        if (buffer_size < 2U) {
            mylite_execution_set_runtime_error(database, "failed to format CONV() value");
            return MYLITE_ERROR;
        }
        buffer[0] = '-';
        return mylite_execution_scalar_binary_format_base_conversion_value(
            database,
            magnitude,
            output.base,
            buffer + 1U,
            buffer_size - 1U
        );
    }
    return mylite_execution_scalar_binary_format_base_conversion_value(
        database,
        magnitude,
        output.base,
        buffer,
        buffer_size
    );
}

static void result_error_from_mylite_status(sqlite3_context *context, int status) {
    if (status == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    sqlite3_result_error(context, "MyLite base conversion function failed", -1);
}
