#include "mylite_weight_string.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    mysql_warning_truncated_wrong_value = 1292,
    weight_string_printable_ascii_min = 0x20U,
    weight_string_printable_ascii_max = 0x7eU,
    weight_string_warning_input_capacity = 96,
};

struct weight_string_truncation_warning {
    const void *input;
    size_t input_size;
    int64_t binary_length;
};

static void weight_string_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void weight_string_binary_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static void weight_string_sqlite_result(
    sqlite3_context *context,
    unsigned char *bytes,
    size_t size,
    bool is_null,
    int rc
);
static int append_weight_string_binary_truncation_warning(
    struct mylite_db *database,
    const struct weight_string_truncation_warning *warning
);

int mylite_weight_string_value(
    struct mylite_db *database,
    const void *input,
    size_t input_size,
    bool input_is_null,
    bool has_binary_length,
    int64_t binary_length,
    unsigned char **out_bytes,
    size_t *out_size,
    bool *out_is_null
) {
    const unsigned char *source = input;
    unsigned char *result = NULL;
    size_t result_size = input_size;

    if ((input == NULL && input_size != 0U) || out_bytes == NULL || out_size == NULL ||
        out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_bytes = NULL;
    *out_size = 0U;
    *out_is_null = false;
    if (input_is_null) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (has_binary_length) {
        if (binary_length < 0 || (uint64_t)binary_length > (uint64_t)(SIZE_MAX - 1U)) {
            return MYLITE_NOMEM;
        }
        result_size = (size_t)binary_length;
        if (input_size > result_size) {
            struct weight_string_truncation_warning warning = {
                .input = input,
                .input_size = input_size,
                .binary_length = binary_length,
            };
            int rc = append_weight_string_binary_truncation_warning(database, &warning);

            if (rc != MYLITE_OK) {
                return rc;
            }
        }
    }
    result = (unsigned char *)calloc(result_size + 1U, 1U);
    if (result == NULL) {
        return MYLITE_NOMEM;
    }
    if (source != NULL && input_size != 0U) {
        size_t copy_size = input_size < result_size ? input_size : result_size;

        memcpy(result, source, copy_size);
    }
    *out_bytes = result;
    *out_size = result_size;
    return MYLITE_OK;
}

int mylite_sqlite_register_weight_string_functions(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_weight_string",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = weight_string_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_weight_string_binary",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = weight_string_binary_sqlite_callback,
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

static void weight_string_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    unsigned char *result = NULL;
    size_t result_size = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (database == NULL || argc != 1 || argv == NULL) {
        sqlite3_result_error(context, "MyLite WEIGHT_STRING() callback failed", -1);
        return;
    }
    rc = mylite_weight_string_value(
        database,
        sqlite3_value_blob(argv[0]),
        (size_t)sqlite3_value_bytes(argv[0]),
        sqlite3_value_type(argv[0]) == SQLITE_NULL,
        false,
        0,
        &result,
        &result_size,
        &is_null
    );
    weight_string_sqlite_result(context, result, result_size, is_null, rc);
}

static void weight_string_binary_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);
    unsigned char *result = NULL;
    size_t result_size = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (database == NULL || argc != 2 || argv == NULL) {
        sqlite3_result_error(context, "MyLite WEIGHT_STRING() callback failed", -1);
        return;
    }
    rc = mylite_weight_string_value(
        database,
        sqlite3_value_blob(argv[0]),
        (size_t)sqlite3_value_bytes(argv[0]),
        sqlite3_value_type(argv[0]) == SQLITE_NULL,
        true,
        sqlite3_value_int64(argv[1]),
        &result,
        &result_size,
        &is_null
    );
    weight_string_sqlite_result(context, result, result_size, is_null, rc);
}

static void weight_string_sqlite_result(
    sqlite3_context *context,
    unsigned char *bytes,
    size_t size,
    bool is_null,
    int rc
) {
    if (rc != MYLITE_OK) {
        free(bytes);
        sqlite3_result_error(context, "WEIGHT_STRING() failed", -1);
        return;
    }
    if (is_null) {
        free(bytes);
        sqlite3_result_null(context);
        return;
    }
    if (size > (size_t)INT_MAX) {
        free(bytes);
        sqlite3_result_error(context, "WEIGHT_STRING() result is too large", -1);
        return;
    }
    sqlite3_result_blob(context, bytes, (int)size, SQLITE_TRANSIENT);
    free(bytes);
}

static int append_weight_string_binary_truncation_warning(
    struct mylite_db *database,
    const struct weight_string_truncation_warning *warning
) {
    const unsigned char *bytes = NULL;
    char printable[weight_string_warning_input_capacity];
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t limit = 0U;
    int written = 0;

    if (database == NULL || warning == NULL ||
        (warning->input == NULL && warning->input_size != 0U)) {
        return MYLITE_MISUSE;
    }
    bytes = warning->input;
    limit = warning->input_size;
    if (limit > sizeof(printable) - 1U) {
        limit = sizeof(printable) - 1U;
    }
    for (size_t index = 0U; index < limit; ++index) {
        unsigned char byte = bytes[index];

        if (byte >= weight_string_printable_ascii_min &&
            byte <= weight_string_printable_ascii_max) {
            printable[index] = (char)byte;
        } else {
            printable[index] = '?';
        }
    }
    printable[limit] = '\0';
    written = snprintf(
        message,
        sizeof(message),
        "Truncated incorrect BINARY(%" PRId64 ") value: '%s'",
        warning->binary_length,
        printable
    );
    if (written < 0 || (size_t)written >= sizeof(message)) {
        return MYLITE_NOMEM;
    }
    return mylite_diagnostics_append_warning(
        &database->diagnostics,
        mysql_warning_truncated_wrong_value,
        "HY000",
        message
    );
}
