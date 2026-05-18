#include "mylite_string_replace.h"

#include "mylite_sqlite_registration.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct replace_slice {
    const char *bytes;
    size_t length;
};

struct replace_inputs {
    struct replace_slice value;
    struct replace_slice search;
    struct replace_slice replacement;
};

static void string_replace_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static int replace_result_length(const struct replace_inputs *inputs, size_t *out_result_length);
static char *build_replace_result(const struct replace_inputs *inputs, size_t result_length);
static bool bytes_at_match(
    const struct replace_slice *value,
    size_t offset,
    const struct replace_slice *search
);

int mylite_string_replace_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    const char *search,
    size_t search_length,
    const char *replacement,
    size_t replacement_length,
    char **out_text,
    size_t *out_text_length
) {
    char *result = NULL;
    struct replace_inputs inputs = {
        .value = {.bytes = value, .length = value_length},
        .search = {.bytes = search, .length = search_length},
        .replacement = {.bytes = replacement, .length = replacement_length},
    };
    size_t result_length = 0U;
    int rc = MYLITE_OK;

    (void)database;
    if (value == NULL || search == NULL || replacement == NULL || out_text == NULL ||
        out_text_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;

    if (inputs.search.length == 0U) {
        result_length = inputs.value.length;
    } else {
        rc = replace_result_length(&inputs, &result_length);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    if (result_length == SIZE_MAX) {
        return MYLITE_NOMEM;
    }

    if (inputs.search.length == 0U) {
        result = (char *)malloc(result_length + 1U);
        if (result == NULL) {
            return MYLITE_NOMEM;
        }
        if (result_length != 0U) {
            memcpy(result, inputs.value.bytes, result_length);
        }
        result[result_length] = '\0';
    } else {
        result = build_replace_result(&inputs, result_length);
        if (result == NULL) {
            return MYLITE_NOMEM;
        }
    }

    *out_text = result;
    *out_text_length = result_length;
    return MYLITE_OK;
}

int mylite_sqlite_register_string_replace_function(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_replace",
            .argument_count = 3,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = string_replace_sqlite_callback,
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

static void string_replace_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    const unsigned char *value = NULL;
    const unsigned char *search = NULL;
    const unsigned char *replacement = NULL;
    int value_length = 0;
    int search_length = 0;
    int replacement_length = 0;
    char *result = NULL;
    size_t result_length = 0U;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 3 || argv == NULL || argv[0] == NULL || argv[1] == NULL ||
        argv[2] == NULL) {
        sqlite3_result_error(context, "invalid MyLite REPLACE callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL ||
        sqlite3_value_type(argv[2]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    value = sqlite3_value_text(argv[0]);
    search = sqlite3_value_text(argv[1]);
    replacement = sqlite3_value_text(argv[2]);
    value_length = sqlite3_value_bytes(argv[0]);
    search_length = sqlite3_value_bytes(argv[1]);
    replacement_length = sqlite3_value_bytes(argv[2]);
    if (value == NULL || search == NULL || replacement == NULL || value_length < 0 ||
        search_length < 0 || replacement_length < 0) {
        sqlite3_result_error_nomem(context);
        return;
    }

    rc = mylite_string_replace_value(
        NULL,
        (const char *)value,
        (size_t)value_length,
        (const char *)search,
        (size_t)search_length,
        (const char *)replacement,
        (size_t)replacement_length,
        &result,
        &result_length
    );
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
        } else {
            sqlite3_result_error(context, "MyLite REPLACE failed", -1);
        }
        free(result);
        return;
    }

    sqlite3_result_text64(
        context,
        result,
        (sqlite3_uint64)result_length,
        SQLITE_TRANSIENT,
        SQLITE_UTF8
    );
    free(result);
}

static int replace_result_length(const struct replace_inputs *inputs, size_t *out_result_length) {
    size_t result_length = 0U;
    size_t offset = 0U;

    if (inputs == NULL || inputs->value.bytes == NULL || inputs->search.bytes == NULL ||
        inputs->search.length == 0U || out_result_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result_length = 0U;

    while (offset < inputs->value.length) {
        if (bytes_at_match(&inputs->value, offset, &inputs->search)) {
            if (result_length > SIZE_MAX - inputs->replacement.length) {
                return MYLITE_NOMEM;
            }
            result_length += inputs->replacement.length;
            offset += inputs->search.length;
        } else {
            if (result_length == SIZE_MAX) {
                return MYLITE_NOMEM;
            }
            ++result_length;
            ++offset;
        }
    }

    *out_result_length = result_length;
    return MYLITE_OK;
}

static char *build_replace_result(const struct replace_inputs *inputs, size_t result_length) {
    char *result = (char *)malloc(result_length + 1U);
    size_t input_offset = 0U;
    size_t output_offset = 0U;

    if (result == NULL) {
        return NULL;
    }

    while (input_offset < inputs->value.length) {
        if (bytes_at_match(&inputs->value, input_offset, &inputs->search)) {
            if (inputs->replacement.length != 0U) {
                memcpy(
                    &result[output_offset],
                    inputs->replacement.bytes,
                    inputs->replacement.length
                );
            }
            output_offset += inputs->replacement.length;
            input_offset += inputs->search.length;
        } else {
            result[output_offset] = inputs->value.bytes[input_offset];
            ++output_offset;
            ++input_offset;
        }
    }

    result[result_length] = '\0';
    return result;
}

static bool bytes_at_match(
    const struct replace_slice *value,
    size_t offset,
    const struct replace_slice *search
) {
    if (value == NULL || search == NULL || value->bytes == NULL || search->bytes == NULL) {
        return false;
    }
    if (offset > value->length || search->length > value->length - offset) {
        return false;
    }
    if (memcmp(&value->bytes[offset], search->bytes, search->length) != 0) {
        return false;
    }
    return true;
}
