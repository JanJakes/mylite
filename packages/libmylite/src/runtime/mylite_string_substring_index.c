#include "mylite_string_substring_index.h"

#include "mylite_sqlite_registration.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct substring_index_slice {
    const char *bytes;
    size_t length;
};

static void substring_index_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static int substring_index_positive_count(
    const struct substring_index_slice *value,
    const struct substring_index_slice *delimiter,
    uint64_t count,
    char **out_text,
    size_t *out_text_length
);
static int substring_index_negative_count(
    const struct substring_index_slice *value,
    const struct substring_index_slice *delimiter,
    uint64_t count,
    char **out_text,
    size_t *out_text_length
);
static bool find_next_delimiter(
    const struct substring_index_slice *value,
    const struct substring_index_slice *delimiter,
    size_t start,
    size_t *out_offset
);
static bool find_previous_delimiter(
    const struct substring_index_slice *value,
    const struct substring_index_slice *delimiter,
    size_t end,
    size_t *out_offset
);
static bool delimiter_matches_at(
    const struct substring_index_slice *value,
    const struct substring_index_slice *delimiter,
    size_t offset
);
static int copy_substring_index_slice(const char *bytes, size_t length, char **out_text);
static uint64_t int64_magnitude(int64_t value);

int mylite_string_substring_index_value(
    struct mylite_db *database,
    const char *value,
    size_t value_length,
    const char *delimiter,
    size_t delimiter_length,
    int64_t count,
    char **out_text,
    size_t *out_text_length
) {
    struct substring_index_slice value_slice = {.bytes = value, .length = value_length};
    struct substring_index_slice delimiter_slice = {
        .bytes = delimiter,
        .length = delimiter_length,
    };

    (void)database;
    if (value == NULL || delimiter == NULL || out_text == NULL || out_text_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_length = 0U;

    if (count == 0 || delimiter_length == 0U) {
        return copy_substring_index_slice("", 0U, out_text);
    }
    if (count > 0) {
        return substring_index_positive_count(
            &value_slice,
            &delimiter_slice,
            (uint64_t)count,
            out_text,
            out_text_length
        );
    }
    return substring_index_negative_count(
        &value_slice,
        &delimiter_slice,
        int64_magnitude(count),
        out_text,
        out_text_length
    );
}

int mylite_sqlite_register_string_substring_index_function(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_substring_index",
            .argument_count = 3,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = substring_index_sqlite_callback,
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

static void substring_index_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    const unsigned char *value = NULL;
    const unsigned char *delimiter = NULL;
    int value_length = 0;
    int delimiter_length = 0;
    int64_t count = 0;
    char *result = NULL;
    size_t result_length = 0U;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 3 || argv == NULL || argv[0] == NULL || argv[1] == NULL ||
        argv[2] == NULL) {
        sqlite3_result_error(context, "invalid MyLite SUBSTRING_INDEX callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL ||
        sqlite3_value_type(argv[2]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    value = sqlite3_value_text(argv[0]);
    delimiter = sqlite3_value_text(argv[1]);
    if (value == NULL || delimiter == NULL) {
        sqlite3_result_error_nomem(context);
        return;
    }
    value_length = sqlite3_value_bytes(argv[0]);
    delimiter_length = sqlite3_value_bytes(argv[1]);
    if (value_length < 0 || delimiter_length < 0) {
        sqlite3_result_error(context, "invalid MyLite SUBSTRING_INDEX callback text length", -1);
        return;
    }
    count = sqlite3_value_int64(argv[2]);

    rc = mylite_string_substring_index_value(
        NULL,
        (const char *)value,
        (size_t)value_length,
        (const char *)delimiter,
        (size_t)delimiter_length,
        count,
        &result,
        &result_length
    );
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK) {
        sqlite3_result_error(context, "MyLite SUBSTRING_INDEX failed", -1);
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

static int substring_index_positive_count(
    const struct substring_index_slice *value,
    const struct substring_index_slice *delimiter,
    uint64_t count,
    char **out_text,
    size_t *out_text_length
) {
    size_t match_offset = 0U;
    size_t search_offset = 0U;

    while (count != 0U) {
        if (!find_next_delimiter(value, delimiter, search_offset, &match_offset)) {
            *out_text_length = value->length;
            return copy_substring_index_slice(value->bytes, value->length, out_text);
        }
        --count;
        if (count == 0U) {
            *out_text_length = match_offset;
            return copy_substring_index_slice(value->bytes, match_offset, out_text);
        }
        search_offset = match_offset + delimiter->length;
    }

    *out_text_length = value->length;
    return copy_substring_index_slice(value->bytes, value->length, out_text);
}

static int substring_index_negative_count(
    const struct substring_index_slice *value,
    const struct substring_index_slice *delimiter,
    uint64_t count,
    char **out_text,
    size_t *out_text_length
) {
    size_t match_offset = 0U;
    size_t search_end = value->length;

    while (count != 0U) {
        if (!find_previous_delimiter(value, delimiter, search_end, &match_offset)) {
            *out_text_length = value->length;
            return copy_substring_index_slice(value->bytes, value->length, out_text);
        }
        --count;
        if (count == 0U) {
            size_t result_offset = match_offset + delimiter->length;
            size_t result_length = value->length - result_offset;

            *out_text_length = result_length;
            return copy_substring_index_slice(
                value->bytes + result_offset,
                result_length,
                out_text
            );
        }
        search_end = match_offset;
    }

    *out_text_length = value->length;
    return copy_substring_index_slice(value->bytes, value->length, out_text);
}

static bool find_next_delimiter(
    const struct substring_index_slice *value,
    const struct substring_index_slice *delimiter,
    size_t start,
    size_t *out_offset
) {
    size_t last_start = 0U;

    if (value == NULL || delimiter == NULL || out_offset == NULL || delimiter->length == 0U ||
        value->length < delimiter->length) {
        return false;
    }
    last_start = value->length - delimiter->length;
    if (start > last_start) {
        return false;
    }
    for (size_t offset = start; offset <= last_start; ++offset) {
        if (delimiter_matches_at(value, delimiter, offset)) {
            *out_offset = offset;
            return true;
        }
    }
    return false;
}

static bool find_previous_delimiter(
    const struct substring_index_slice *value,
    const struct substring_index_slice *delimiter,
    size_t end,
    size_t *out_offset
) {
    size_t last_start = 0U;

    if (value == NULL || delimiter == NULL || out_offset == NULL || delimiter->length == 0U ||
        end < delimiter->length) {
        return false;
    }
    last_start = end - delimiter->length;
    if (last_start > value->length - delimiter->length) {
        last_start = value->length - delimiter->length;
    }
    for (size_t offset = last_start + 1U; offset != 0U; --offset) {
        size_t match_offset = offset - 1U;

        if (delimiter_matches_at(value, delimiter, match_offset)) {
            *out_offset = match_offset;
            return true;
        }
    }
    return false;
}

static bool delimiter_matches_at(
    const struct substring_index_slice *value,
    const struct substring_index_slice *delimiter,
    size_t offset
) {
    if (value == NULL || delimiter == NULL || delimiter->length == 0U || offset > value->length ||
        delimiter->length > value->length - offset) {
        return false;
    }
    return memcmp(value->bytes + offset, delimiter->bytes, delimiter->length) == 0;
}

static int copy_substring_index_slice(const char *bytes, size_t length, char **out_text) {
    char *result = NULL;

    if (bytes == NULL || out_text == NULL) {
        return MYLITE_MISUSE;
    }
    if (length == SIZE_MAX) {
        return MYLITE_NOMEM;
    }

    result = (char *)malloc(length + 1U);
    if (result == NULL) {
        return MYLITE_NOMEM;
    }
    if (length != 0U) {
        memcpy(result, bytes, length);
    }
    result[length] = '\0';
    *out_text = result;
    return MYLITE_OK;
}

static uint64_t int64_magnitude(int64_t value) {
    if (value == INT64_MIN) {
        return (uint64_t)INT64_MAX + 1U;
    }
    if (value < 0) {
        return (uint64_t)(-value);
    }
    return (uint64_t)value;
}
