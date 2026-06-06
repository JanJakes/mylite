#include "mylite_string_case.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <stdint.h>
#include <stdlib.h>

enum {
    mysql_error_parse = 1064,
    ascii_upper_a = 'A',
    ascii_upper_z = 'Z',
    ascii_lower_a = 'a',
    ascii_lower_z = 'z',
    ascii_case_delta = 'a' - 'A',
    ascii_max = 0x7f,
};

static void string_case_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static bool string_case_kind_from_sqlite_context(
    sqlite3_context *context,
    enum mylite_string_case_kind *out_kind
);
static void set_string_case_unsupported_error(struct mylite_db *database);
static bool ascii_string_case_byte(
    unsigned char input,
    enum mylite_string_case_kind case_kind,
    char *out_byte
);

int mylite_string_case_ascii_value(
    struct mylite_db *database,
    enum mylite_string_case_kind case_kind,
    const char *value,
    size_t value_length,
    char **out_text
) {
    char *result = NULL;

    if (value == NULL || out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (value_length == SIZE_MAX) {
        set_string_case_unsupported_error(database);
        return MYLITE_ERROR;
    }
    result = (char *)malloc(value_length + 1U);
    if (result == NULL) {
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < value_length; ++index) {
        if (!ascii_string_case_byte((unsigned char)value[index], case_kind, &result[index])) {
            free(result);
            set_string_case_unsupported_error(database);
            return MYLITE_ERROR;
        }
    }
    result[value_length] = '\0';
    *out_text = result;
    return MYLITE_OK;
}

int mylite_sqlite_register_string_case_functions(sqlite3 *sqlite) {
    static const enum mylite_string_case_kind lower_kind = MYLITE_STRING_CASE_LOWER;
    static const enum mylite_string_case_kind upper_kind = MYLITE_STRING_CASE_UPPER;
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_lower_ascii",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&lower_kind,
            .scalar_callback = string_case_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_upper_ascii",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = (void *)&upper_kind,
            .scalar_callback = string_case_sqlite_callback,
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

static void string_case_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_db *database = NULL;
    enum mylite_string_case_kind case_kind = MYLITE_STRING_CASE_LOWER;
    const unsigned char *value = NULL;
    int value_length = 0;
    char *result = NULL;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite string case callback", -1);
        return;
    }
    if (!string_case_kind_from_sqlite_context(context, &case_kind)) {
        sqlite3_result_error(context, "invalid MyLite string case kind", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    database = mylite_sqlite_bootstrap_owner_from_context(context);
    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite string case owner", -1);
        return;
    }

    value = sqlite3_value_text(argv[0]);
    value_length = sqlite3_value_bytes(argv[0]);
    if (value == NULL || value_length < 0) {
        sqlite3_result_error_nomem(context);
        return;
    }

    rc = mylite_string_case_ascii_value(
        database,
        case_kind,
        (const char *)value,
        (size_t)value_length,
        &result
    );
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
        } else {
            sqlite3_result_error(context, "MyLite string case conversion failed", -1);
        }
        free(result);
        return;
    }

    sqlite3_result_text(context, result, -1, SQLITE_TRANSIENT);
    free(result);
}

static bool string_case_kind_from_sqlite_context(
    sqlite3_context *context,
    enum mylite_string_case_kind *out_kind
) {
    const enum mylite_string_case_kind *case_kind = NULL;

    if (context == NULL || out_kind == NULL) {
        return false;
    }
    case_kind = (const enum mylite_string_case_kind *)sqlite3_user_data(context);
    if (case_kind == NULL ||
        (*case_kind != MYLITE_STRING_CASE_LOWER && *case_kind != MYLITE_STRING_CASE_UPPER)) {
        return false;
    }
    *out_kind = *case_kind;
    return true;
}

static void set_string_case_unsupported_error(struct mylite_db *database) {
    if (database == NULL) {
        return;
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_parse,
        "42000",
        "string case functions support only ASCII text values"
    );
}

static bool ascii_string_case_byte(
    unsigned char input,
    enum mylite_string_case_kind case_kind,
    char *out_byte
) {
    unsigned char output = input;

    if (out_byte == NULL || input == '\0' || input > ascii_max) {
        return false;
    }
    if (case_kind == MYLITE_STRING_CASE_LOWER && input >= ascii_upper_a && input <= ascii_upper_z) {
        output = (unsigned char)(input + ascii_case_delta);
    } else if (case_kind == MYLITE_STRING_CASE_UPPER && input >= ascii_lower_a &&
               input <= ascii_lower_z) {
        output = (unsigned char)(input - ascii_case_delta);
    } else if (case_kind != MYLITE_STRING_CASE_LOWER && case_kind != MYLITE_STRING_CASE_UPPER) {
        return false;
    }
    *out_byte = (char)output;
    return true;
}
