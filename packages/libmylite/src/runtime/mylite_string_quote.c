#include "mylite_string_quote.h"

#include "mylite_sqlite_registration.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    quote_extra_bytes = 3,
    ascii_nul = 0x00,
    ascii_control_z = 0x1a,
};

static void string_quote_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static size_t append_quoted_byte(unsigned char byte, char *result, size_t offset);

int mylite_string_quote_sql_value(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    bool is_null,
    char **out_text,
    size_t *out_text_length
) {
    static const char null_text[] = "NULL";
    char *result = NULL;
    size_t offset = 0U;

    (void)database;
    if (out_text == NULL || out_text_length == NULL || (!is_null && text == NULL)) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;

    if (is_null) {
        result = (char *)malloc(sizeof(null_text));
        if (result == NULL) {
            return MYLITE_NOMEM;
        }
        memcpy(result, null_text, sizeof(null_text));
        *out_text = result;
        *out_text_length = sizeof(null_text) - 1U;
        return MYLITE_OK;
    }

    if (text_length > (SIZE_MAX - quote_extra_bytes) / 2U) {
        return MYLITE_NOMEM;
    }

    result = (char *)malloc((text_length * 2U) + quote_extra_bytes);
    if (result == NULL) {
        return MYLITE_NOMEM;
    }

    result[offset++] = '\'';
    for (size_t index = 0U; index < text_length; ++index) {
        offset = append_quoted_byte((unsigned char)text[index], result, offset);
    }
    result[offset++] = '\'';
    result[offset] = '\0';

    *out_text = result;
    *out_text_length = offset;
    return MYLITE_OK;
}

int mylite_sqlite_register_string_quote_function(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_quote_sql",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = string_quote_sqlite_callback,
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

static void string_quote_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const unsigned char *text = NULL;
    int text_length = 0;
    char *result = NULL;
    size_t result_length = 0U;
    int rc = MYLITE_OK;
    bool is_null = false;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite QUOTE callback", -1);
        return;
    }

    is_null = sqlite3_value_type(argv[0]) == SQLITE_NULL;
    if (!is_null) {
        text = sqlite3_value_text(argv[0]);
        text_length = sqlite3_value_bytes(argv[0]);
        if (text == NULL || text_length < 0) {
            sqlite3_result_error_nomem(context);
            return;
        }
    }

    rc = mylite_string_quote_sql_value(
        NULL,
        (const char *)text,
        (size_t)text_length,
        is_null,
        &result,
        &result_length
    );
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
        } else {
            sqlite3_result_error(context, "MyLite QUOTE failed", -1);
        }
        free(result);
        return;
    }

    sqlite3_result_text64(context, result, (sqlite3_uint64)result_length, free, SQLITE_UTF8);
}

static size_t append_quoted_byte(unsigned char byte, char *result, size_t offset) {
    switch (byte) {
    case ascii_nul:
        result[offset++] = '\\';
        result[offset++] = '0';
        break;
    case ascii_control_z:
        result[offset++] = '\\';
        result[offset++] = 'Z';
        break;
    case '\\':
        result[offset++] = '\\';
        result[offset++] = '\\';
        break;
    case '\'':
        result[offset++] = '\\';
        result[offset++] = '\'';
        break;
    default:
        result[offset++] = (char)byte;
        break;
    }
    return offset;
}
