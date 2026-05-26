#include "mylite_string_bitmask.h"

#include "mylite_sqlite_registration.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    export_set_default_bit_count = 64,
    export_set_min_sqlite_argument_count = 3,
    export_set_max_sqlite_argument_count = 5,
    make_set_max_value_count = export_set_default_bit_count,
};

struct export_set_result_shape {
    uint64_t bits;
    size_t bit_count;
    struct mylite_string_bitmask_slice on;
    struct mylite_string_bitmask_slice off;
    struct mylite_string_bitmask_slice separator;
};

struct make_set_result_shape {
    uint64_t bits;
    const struct mylite_string_bitmask_slice *values;
    size_t value_count;
};

struct make_set_length_result {
    size_t result_length;
    size_t selected_count;
};

static void string_export_set_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static void string_make_set_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static struct mylite_string_bitmask_slice sqlite_value_text_slice(sqlite3_value *value);
static void sqlite_result_from_string_bitmask_value(
    sqlite3_context *context,
    int rc,
    char *result,
    size_t result_length,
    bool is_null,
    const char *error_message
);
static size_t export_set_effective_bit_count(int64_t number_of_bits, bool has_number_of_bits);
static int export_set_result_length(struct export_set_result_shape shape, size_t *out_length);
static int make_set_result_length(
    struct make_set_result_shape shape,
    struct make_set_length_result *out_result
);
static void copy_make_set_result(char *result, struct make_set_result_shape shape);
static size_t make_set_effective_value_count(size_t value_count);
static bool make_set_value_is_selected(struct make_set_result_shape shape, size_t value_index);
static bool string_bitmask_slice_has_invalid_text(const struct mylite_string_bitmask_slice *slice);
static int copy_empty_string(char **out_text);
static int checked_add_size(size_t left, size_t right, size_t *inout_value);

int mylite_string_export_set_value(
    uint64_t bits,
    bool bits_is_null,
    struct mylite_string_bitmask_slice on,
    struct mylite_string_bitmask_slice off,
    struct mylite_string_bitmask_slice separator,
    int64_t number_of_bits,
    bool number_of_bits_is_null,
    bool has_number_of_bits,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    char *result = NULL;
    size_t bit_count = 0U;
    size_t result_length = 0U;
    size_t offset = 0U;
    int rc = MYLITE_OK;

    if ((on.text == NULL && on.length != 0U) || (off.text == NULL && off.length != 0U) ||
        (separator.text == NULL && separator.length != 0U) || out_text == NULL ||
        out_text_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    if (bits_is_null || on.is_null || off.is_null || separator.is_null || number_of_bits_is_null) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    bit_count = export_set_effective_bit_count(number_of_bits, has_number_of_bits);
    if (bit_count == 0U) {
        return copy_empty_string(out_text);
    }

    rc = export_set_result_length(
        (struct export_set_result_shape){
            .bits = bits,
            .bit_count = bit_count,
            .on = on,
            .off = off,
            .separator = separator,
        },
        &result_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    result = (char *)malloc(result_length + 1U);
    if (result == NULL) {
        return MYLITE_NOMEM;
    }

    for (size_t bit_index = 0U; bit_index < bit_count; ++bit_index) {
        const struct mylite_string_bitmask_slice value =
            (bits & (UINT64_C(1) << bit_index)) != 0U ? on : off;

        if (bit_index != 0U && separator.length != 0U) {
            memcpy(&result[offset], separator.text, separator.length);
            offset += separator.length;
        }
        if (value.length != 0U) {
            memcpy(&result[offset], value.text, value.length);
            offset += value.length;
        }
    }
    result[offset] = '\0';
    *out_text = result;
    *out_text_length = result_length;
    return MYLITE_OK;
}

int mylite_string_make_set_value(
    uint64_t bits,
    bool bits_is_null,
    const struct mylite_string_bitmask_slice *values,
    size_t value_count,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    char *result = NULL;
    struct make_set_length_result length = {
        .result_length = 0U,
        .selected_count = 0U,
    };
    struct make_set_result_shape shape = {
        .bits = bits,
        .values = values,
        .value_count = value_count,
    };
    int rc = MYLITE_OK;

    if ((values == NULL && value_count != 0U) || out_text == NULL || out_text_length == NULL ||
        out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;

    if (bits_is_null) {
        *out_is_null = true;
        return MYLITE_OK;
    }

    rc = make_set_result_length(shape, &length);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (length.selected_count == 0U) {
        return copy_empty_string(out_text);
    }
    result = (char *)malloc(length.result_length + 1U);
    if (result == NULL) {
        return MYLITE_NOMEM;
    }

    copy_make_set_result(result, shape);
    *out_text = result;
    *out_text_length = length.result_length;
    return MYLITE_OK;
}

int mylite_sqlite_register_string_bitmask_functions(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_export_set",
            .argument_count = -1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = string_export_set_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_make_set",
            .argument_count = -1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = string_make_set_sqlite_callback,
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

static void string_export_set_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    static const struct mylite_string_bitmask_slice default_separator = {
        .text = ",",
        .length = 1U,
        .is_null = false,
    };
    struct mylite_string_bitmask_slice separator = default_separator;
    uint64_t bits = 0U;
    int64_t number_of_bits = 0;
    bool bits_is_null = false;
    bool number_of_bits_is_null = false;
    char *result = NULL;
    size_t result_length = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (context == NULL || argc < export_set_min_sqlite_argument_count ||
        argc > export_set_max_sqlite_argument_count || argv == NULL || argv[0] == NULL ||
        argv[1] == NULL || argv[2] == NULL || (argc >= 4 && argv[3] == NULL) ||
        (argc >= export_set_max_sqlite_argument_count && argv[4] == NULL)) {
        sqlite3_result_error(context, "invalid MyLite EXPORT_SET callback", -1);
        return;
    }

    bits_is_null = sqlite3_value_type(argv[0]) == SQLITE_NULL;
    if (!bits_is_null) {
        bits = (uint64_t)sqlite3_value_int64(argv[0]);
    }
    if (argc >= 4) {
        separator = sqlite_value_text_slice(argv[3]);
    }
    if (argc == export_set_max_sqlite_argument_count) {
        number_of_bits_is_null = sqlite3_value_type(argv[4]) == SQLITE_NULL;
        if (!number_of_bits_is_null) {
            number_of_bits = sqlite3_value_int64(argv[4]);
        }
    }

    rc = mylite_string_export_set_value(
        bits,
        bits_is_null,
        sqlite_value_text_slice(argv[1]),
        sqlite_value_text_slice(argv[2]),
        separator,
        number_of_bits,
        number_of_bits_is_null,
        argc == export_set_max_sqlite_argument_count,
        &result,
        &result_length,
        &is_null
    );
    sqlite_result_from_string_bitmask_value(
        context,
        rc,
        result,
        result_length,
        is_null,
        "invalid MyLite EXPORT_SET result"
    );
}

static void string_make_set_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    struct mylite_string_bitmask_slice *values = NULL;
    uint64_t bits = 0U;
    bool bits_is_null = false;
    char *result = NULL;
    size_t result_length = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (context == NULL || argc < 2 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite MAKE_SET callback", -1);
        return;
    }

    if ((size_t)(argc - 1) > SIZE_MAX / sizeof(*values)) {
        sqlite3_result_error_nomem(context);
        return;
    }
    values = (struct mylite_string_bitmask_slice *)calloc((size_t)(argc - 1), sizeof(*values));
    if (values == NULL) {
        sqlite3_result_error_nomem(context);
        return;
    }

    bits_is_null = sqlite3_value_type(argv[0]) == SQLITE_NULL;
    if (!bits_is_null) {
        bits = (uint64_t)sqlite3_value_int64(argv[0]);
    }
    for (int argument_index = 1; argument_index < argc; ++argument_index) {
        if (argv[argument_index] == NULL) {
            sqlite3_result_error(context, "invalid MyLite MAKE_SET callback", -1);
            free(values);
            return;
        }
        values[argument_index - 1] = sqlite_value_text_slice(argv[argument_index]);
    }

    rc = mylite_string_make_set_value(
        bits,
        bits_is_null,
        values,
        (size_t)(argc - 1),
        &result,
        &result_length,
        &is_null
    );
    free(values);
    sqlite_result_from_string_bitmask_value(
        context,
        rc,
        result,
        result_length,
        is_null,
        "invalid MyLite MAKE_SET result"
    );
}

static struct mylite_string_bitmask_slice sqlite_value_text_slice(sqlite3_value *value) {
    if (value == NULL || sqlite3_value_type(value) == SQLITE_NULL) {
        return (struct mylite_string_bitmask_slice){.text = NULL, .length = 0U, .is_null = true};
    }
    return (struct mylite_string_bitmask_slice){
        .text = (const char *)sqlite3_value_text(value),
        .length = (size_t)sqlite3_value_bytes(value),
        .is_null = false,
    };
}

static void sqlite_result_from_string_bitmask_value(
    sqlite3_context *context,
    int rc,
    char *result,
    size_t result_length,
    bool is_null,
    const char *error_message
) {
    if (rc == MYLITE_NOMEM) {
        free(result);
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK) {
        free(result);
        sqlite3_result_error(context, error_message, -1);
        return;
    }
    if (is_null) {
        free(result);
        sqlite3_result_null(context);
        return;
    }
    if (result_length > (size_t)INT_MAX) {
        free(result);
        sqlite3_result_error(context, error_message, -1);
        return;
    }
    sqlite3_result_text(context, result, (int)result_length, free);
}

static size_t export_set_effective_bit_count(int64_t number_of_bits, bool has_number_of_bits) {
    if (!has_number_of_bits) {
        return export_set_default_bit_count;
    }
    if (number_of_bits < 0 || number_of_bits > export_set_default_bit_count) {
        return export_set_default_bit_count;
    }
    return (size_t)number_of_bits;
}

static int export_set_result_length(struct export_set_result_shape shape, size_t *out_length) {
    size_t result_length = 0U;

    if (out_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_length = 0U;
    for (size_t bit_index = 0U; bit_index < shape.bit_count; ++bit_index) {
        const struct mylite_string_bitmask_slice value =
            (shape.bits & (UINT64_C(1) << bit_index)) != 0U ? shape.on : shape.off;

        if (checked_add_size(result_length, value.length, &result_length) != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        if (bit_index != 0U &&
            checked_add_size(result_length, shape.separator.length, &result_length) != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
    }
    *out_length = result_length;
    return MYLITE_OK;
}

static int make_set_result_length(
    struct make_set_result_shape shape,
    struct make_set_length_result *out_result
) {
    size_t result_length = 0U;
    size_t selected_count = 0U;
    const size_t effective_value_count = make_set_effective_value_count(shape.value_count);

    if (out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = (struct make_set_length_result){
        .result_length = 0U,
        .selected_count = 0U,
    };
    for (size_t value_index = 0U; value_index < effective_value_count; ++value_index) {
        const struct mylite_string_bitmask_slice *value = &shape.values[value_index];

        if (!make_set_value_is_selected(shape, value_index)) {
            continue;
        }
        if (string_bitmask_slice_has_invalid_text(value)) {
            return MYLITE_MISUSE;
        }
        if (checked_add_size(result_length, value->length, &result_length) != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        if (selected_count != 0U &&
            checked_add_size(result_length, 1U, &result_length) != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        ++selected_count;
    }
    *out_result = (struct make_set_length_result){
        .result_length = result_length,
        .selected_count = selected_count,
    };
    return MYLITE_OK;
}

static void copy_make_set_result(char *result, struct make_set_result_shape shape) {
    size_t selected_count = 0U;
    size_t offset = 0U;
    const size_t effective_value_count = make_set_effective_value_count(shape.value_count);

    for (size_t value_index = 0U; value_index < effective_value_count; ++value_index) {
        const struct mylite_string_bitmask_slice *value = &shape.values[value_index];

        if (!make_set_value_is_selected(shape, value_index)) {
            continue;
        }
        if (selected_count != 0U) {
            result[offset] = ',';
            ++offset;
        }
        if (value->length != 0U) {
            memcpy(&result[offset], value->text, value->length);
            offset += value->length;
        }
        ++selected_count;
    }
    result[offset] = '\0';
}

static size_t make_set_effective_value_count(size_t value_count) {
    if (value_count > make_set_max_value_count) {
        return make_set_max_value_count;
    }
    return value_count;
}

static bool make_set_value_is_selected(struct make_set_result_shape shape, size_t value_index) {
    const struct mylite_string_bitmask_slice *value = &shape.values[value_index];

    if ((shape.bits & (UINT64_C(1) << value_index)) == 0U) {
        return false;
    }
    if (value->is_null) {
        return false;
    }
    return true;
}

static bool string_bitmask_slice_has_invalid_text(const struct mylite_string_bitmask_slice *slice) {
    if (slice == NULL || slice->text != NULL || slice->length == 0U) {
        return false;
    }
    return true;
}

static int copy_empty_string(char **out_text) {
    char *result = NULL;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;

    result = (char *)malloc(1U);
    if (result == NULL) {
        return MYLITE_NOMEM;
    }
    result[0] = '\0';
    *out_text = result;
    return MYLITE_OK;
}

static int checked_add_size(size_t left, size_t right, size_t *inout_value) {
    if (inout_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (left > SIZE_MAX - right) {
        return MYLITE_NOMEM;
    }
    *inout_value = left + right;
    return MYLITE_OK;
}
