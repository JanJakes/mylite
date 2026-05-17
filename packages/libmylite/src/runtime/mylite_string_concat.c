#include "mylite_string_concat.h"

#include "mylite_sqlite_registration.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void concat_ws_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static int concat_ws_result_length(
    const struct mylite_string_concat_argument *arguments,
    size_t argument_count,
    const struct mylite_string_concat_argument *separator,
    size_t *out_total_length
);
static int sqlite_concat_ws_arguments(
    int argc,
    sqlite3_value **argv,
    struct mylite_string_concat_argument **out_arguments
);

int mylite_string_concat_ws_value(
    struct mylite_db *database,
    const struct mylite_string_concat_argument *arguments,
    size_t argument_count,
    char **out_text
) {
    const struct mylite_string_concat_argument *separator = NULL;
    char *result = NULL;
    size_t value_count = 0U;
    size_t total_length = 0U;
    size_t offset = 0U;
    int rc = MYLITE_OK;

    (void)database;
    if (arguments == NULL || argument_count < 2U || out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;

    separator = &arguments[0];
    if (separator->is_null) {
        return MYLITE_OK;
    }
    if (separator->text == NULL && separator->text_length != 0U) {
        return MYLITE_MISUSE;
    }

    rc = concat_ws_result_length(arguments, argument_count, separator, &total_length);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (total_length == SIZE_MAX) {
        return MYLITE_NOMEM;
    }
    result = (char *)malloc(total_length + 1U);
    if (result == NULL) {
        return MYLITE_NOMEM;
    }

    value_count = 0U;
    for (size_t argument_index = 1U; argument_index < argument_count; ++argument_index) {
        const struct mylite_string_concat_argument *argument = &arguments[argument_index];

        if (argument->is_null) {
            continue;
        }
        if (value_count != 0U && separator->text_length != 0U) {
            memcpy(&result[offset], separator->text, separator->text_length);
            offset += separator->text_length;
        }
        if (argument->text_length != 0U) {
            memcpy(&result[offset], argument->text, argument->text_length);
            offset += argument->text_length;
        }
        ++value_count;
    }
    result[offset] = '\0';
    *out_text = result;
    return MYLITE_OK;
}

static int concat_ws_result_length(
    const struct mylite_string_concat_argument *arguments,
    size_t argument_count,
    const struct mylite_string_concat_argument *separator,
    size_t *out_total_length
) {
    size_t value_count = 0U;
    size_t total_length = 0U;

    if (arguments == NULL || argument_count < 2U || separator == NULL || out_total_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_total_length = 0U;

    for (size_t argument_index = 1U; argument_index < argument_count; ++argument_index) {
        const struct mylite_string_concat_argument *argument = &arguments[argument_index];

        if (argument->is_null) {
            continue;
        }
        if (argument->text == NULL && argument->text_length != 0U) {
            return MYLITE_MISUSE;
        }
        if (total_length > SIZE_MAX - argument->text_length) {
            return MYLITE_NOMEM;
        }
        total_length += argument->text_length;
        if (value_count != 0U) {
            if (total_length > SIZE_MAX - separator->text_length) {
                return MYLITE_NOMEM;
            }
            total_length += separator->text_length;
        }
        ++value_count;
    }

    *out_total_length = total_length;
    return MYLITE_OK;
}

int mylite_sqlite_register_string_concat_functions(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_concat_ws",
            .argument_count = -1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = concat_ws_sqlite_callback,
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

static void concat_ws_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_string_concat_argument *arguments = NULL;
    char *result = NULL;
    int rc = MYLITE_OK;

    if (context == NULL || argc < 2 || argv == NULL) {
        sqlite3_result_error(context, "invalid MyLite CONCAT_WS callback", -1);
        return;
    }

    rc = sqlite_concat_ws_arguments(argc, argv, &arguments);
    if (rc == MYLITE_OK) {
        rc = mylite_string_concat_ws_value(NULL, arguments, (size_t)argc, &result);
    }
    free(arguments);
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
        } else {
            sqlite3_result_error(context, "MyLite CONCAT_WS failed", -1);
        }
        free(result);
        return;
    }
    if (result == NULL) {
        sqlite3_result_null(context);
        return;
    }

    sqlite3_result_text(context, result, -1, SQLITE_TRANSIENT);
    free(result);
}

static int sqlite_concat_ws_arguments(
    int argc,
    sqlite3_value **argv,
    struct mylite_string_concat_argument **out_arguments
) {
    struct mylite_string_concat_argument *arguments = NULL;

    if (argc < 2 || argv == NULL || out_arguments == NULL) {
        return MYLITE_MISUSE;
    }
    *out_arguments = NULL;

    arguments = (struct mylite_string_concat_argument *)calloc((size_t)argc, sizeof(*arguments));
    if (arguments == NULL) {
        return MYLITE_NOMEM;
    }

    for (int argument_index = 0; argument_index < argc; ++argument_index) {
        int value_type = SQLITE_NULL;
        int value_length = 0;
        const unsigned char *value_text = NULL;

        if (argv[argument_index] == NULL) {
            free(arguments);
            return MYLITE_MISUSE;
        }

        value_type = sqlite3_value_type(argv[argument_index]);
        if (value_type == SQLITE_NULL) {
            arguments[argument_index].is_null = true;
            continue;
        }

        value_text = sqlite3_value_text(argv[argument_index]);
        value_length = sqlite3_value_bytes(argv[argument_index]);
        if (value_text == NULL || value_length < 0) {
            free(arguments);
            return MYLITE_NOMEM;
        }
        arguments[argument_index].text = (const char *)value_text;
        arguments[argument_index].text_length = (size_t)value_length;
    }

    *out_arguments = arguments;
    return MYLITE_OK;
}
