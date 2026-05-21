#include "mylite_string_char.h"

#include "mylite_sqlite_registration.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    char_initial_capacity = 16,
    char_highest_byte_shift = 24,
    char_bits_per_byte = 8,
    char_byte_mask = 0xffU,
};

static void char_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static int char_buffer_append_byte(struct mylite_string_char_buffer *buffer, unsigned char byte);
static int char_buffer_reserve(struct mylite_string_char_buffer *buffer, size_t additional);

int mylite_string_char_buffer_append_int64(
    struct mylite_string_char_buffer *buffer,
    int64_t value
) {
    return mylite_string_char_buffer_append_uint64(buffer, (uint64_t)value);
}

int mylite_string_char_buffer_append_uint64(
    struct mylite_string_char_buffer *buffer,
    uint64_t value
) {
    uint32_t low_bits = (uint32_t)value;
    bool started = false;

    if (buffer == NULL) {
        return MYLITE_MISUSE;
    }
    if (low_bits == 0U) {
        return char_buffer_append_byte(buffer, 0U);
    }

    for (int shift = char_highest_byte_shift; shift >= 0; shift -= char_bits_per_byte) {
        unsigned char byte = (unsigned char)((low_bits >> (unsigned int)shift) & char_byte_mask);

        if (byte != 0U || started) {
            int rc = char_buffer_append_byte(buffer, byte);

            if (rc != MYLITE_OK) {
                return rc;
            }
            started = true;
        }
    }
    return MYLITE_OK;
}

int mylite_string_char_buffer_finish(
    struct mylite_string_char_buffer *buffer,
    char **out_bytes,
    size_t *out_size
) {
    unsigned char *bytes = NULL;

    if (buffer == NULL || out_bytes == NULL || out_size == NULL) {
        return MYLITE_MISUSE;
    }
    *out_bytes = NULL;
    *out_size = 0U;

    if (buffer->bytes == NULL) {
        bytes = malloc(1U);
        if (bytes == NULL) {
            return MYLITE_NOMEM;
        }
        bytes[0] = '\0';
    } else {
        if (char_buffer_reserve(buffer, 1U) != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        buffer->bytes[buffer->size] = '\0';
        bytes = buffer->bytes;
        buffer->bytes = NULL;
        buffer->capacity = 0U;
    }

    *out_bytes = (char *)bytes;
    *out_size = buffer->size;
    buffer->size = 0U;
    return MYLITE_OK;
}

void mylite_string_char_buffer_deinit(struct mylite_string_char_buffer *buffer) {
    if (buffer == NULL) {
        return;
    }
    free(buffer->bytes);
    memset(buffer, 0, sizeof(*buffer));
}

int mylite_sqlite_register_string_char_function(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_char",
            .argument_count = -1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = char_sqlite_callback,
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

static void char_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_string_char_buffer buffer = {0};
    char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = MYLITE_OK;

    if (context == NULL || argc < 1 || argv == NULL) {
        sqlite3_result_error(context, "invalid MyLite CHAR callback", -1);
        return;
    }

    for (int index = 0; rc == MYLITE_OK && index < argc; ++index) {
        if (argv[index] == NULL) {
            sqlite3_result_error(context, "invalid MyLite CHAR argument", -1);
            mylite_string_char_buffer_deinit(&buffer);
            return;
        }
        if (sqlite3_value_type(argv[index]) != SQLITE_NULL) {
            rc = mylite_string_char_buffer_append_int64(
                &buffer,
                (int64_t)sqlite3_value_int64(argv[index])
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_string_char_buffer_finish(&buffer, &bytes, &byte_count);
    }
    if (rc == MYLITE_NOMEM) {
        mylite_string_char_buffer_deinit(&buffer);
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK || byte_count > (size_t)INT_MAX) {
        free(bytes);
        mylite_string_char_buffer_deinit(&buffer);
        sqlite3_result_error(context, "MyLite CHAR conversion failed", -1);
        return;
    }

    sqlite3_result_blob(context, bytes, (int)byte_count, SQLITE_TRANSIENT);
    free(bytes);
}

static int char_buffer_append_byte(struct mylite_string_char_buffer *buffer, unsigned char byte) {
    int rc = char_buffer_reserve(buffer, 1U);

    if (rc != MYLITE_OK) {
        return rc;
    }
    buffer->bytes[buffer->size] = byte;
    ++buffer->size;
    return MYLITE_OK;
}

static int char_buffer_reserve(struct mylite_string_char_buffer *buffer, size_t additional) {
    unsigned char *bytes = NULL;
    size_t required = 0U;
    size_t capacity = 0U;

    if (buffer == NULL) {
        return MYLITE_MISUSE;
    }
    if (additional > SIZE_MAX - buffer->size) {
        return MYLITE_NOMEM;
    }
    required = buffer->size + additional;
    if (required <= buffer->capacity) {
        return MYLITE_OK;
    }

    capacity = buffer->capacity == 0U ? char_initial_capacity : buffer->capacity;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2U) {
            capacity = required;
            break;
        }
        capacity *= 2U;
    }

    bytes = realloc(buffer->bytes, capacity);
    if (bytes == NULL) {
        return MYLITE_NOMEM;
    }
    buffer->bytes = bytes;
    buffer->capacity = capacity;
    return MYLITE_OK;
}
