#include "mylite_random_bytes.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <mylite/mylite.h>

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#  include <Windows.h>
#  include <bcrypt.h>
#else
#  include <fcntl.h>
#  include <unistd.h>
#  ifdef __linux__
#    include <sys/random.h>
#  endif
#endif

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__FreeBSD__) && !defined(__OpenBSD__) &&   \
    !defined(__NetBSD__) && !defined(__DragonFly__)
#  define MYLITE_RANDOM_BYTES_HAS_URANDOM_FALLBACK 1
#endif

enum {
    mysql_error_random_bytes_length_out_of_range = 1690,
    mysql_warning_truncated_incorrect_integer = 1292,
    random_bytes_min_length = 1,
    random_bytes_max_length = 1024,
    random_bytes_warning_input_capacity = 96,
    random_bytes_printable_ascii_min = 0x20,
    random_bytes_printable_ascii_max = 0x7e,
};

static const double random_bytes_round_half_increment = 0.5;

static void random_bytes_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static int random_bytes_length_from_sqlite_value(
    sqlite3_context *context,
    sqlite3_value *value,
    size_t *out_length,
    bool *out_is_null
);
static int random_bytes_length_from_finite_double(
    struct mylite_db *database,
    double value,
    size_t *out_length
);
static int append_truncated_incorrect_integer_warning(
    struct mylite_db *database,
    const void *input,
    size_t input_size
);
static int fill_random_bytes(unsigned char *bytes, size_t length);
#ifdef _WIN32
static int fill_random_bytes_windows(unsigned char *bytes, size_t length);
#else
#  if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || \
      defined(__DragonFly__)
static int fill_random_bytes_arc4random(unsigned char *bytes, size_t length);
#  endif
#  ifdef __linux__
static int fill_random_bytes_getrandom(unsigned char *bytes, size_t length, bool *out_unavailable);
#  endif
#  ifdef MYLITE_RANDOM_BYTES_HAS_URANDOM_FALLBACK
static int fill_random_bytes_from_urandom(unsigned char *bytes, size_t length);
#  endif
#endif
static void format_warning_input(
    const void *input,
    size_t input_size,
    char destination[random_bytes_warning_input_capacity]
);
static char warning_input_printable_byte(unsigned char byte);
static bool input_has_nonspace_suffix(const char *start, const char *end);
static bool warning_input_byte_is_printable(unsigned char byte);

int mylite_random_bytes_generate(size_t length, unsigned char **out_bytes) {
    unsigned char *bytes = NULL;

    if (out_bytes == NULL) {
        return MYLITE_MISUSE;
    }
    *out_bytes = NULL;
    if (length < random_bytes_min_length || length > random_bytes_max_length ||
        length > (size_t)INT_MAX) {
        return MYLITE_MISUSE;
    }

    bytes = (unsigned char *)malloc(length);
    if (bytes == NULL) {
        return MYLITE_NOMEM;
    }
    if (fill_random_bytes(bytes, length) != MYLITE_OK) {
        free(bytes);
        return MYLITE_ERROR;
    }
    *out_bytes = bytes;
    return MYLITE_OK;
}

int mylite_random_bytes_length_from_int64(
    struct mylite_db *database,
    int64_t value,
    size_t *out_length
) {
    if (out_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_length = 0U;
    if (value < random_bytes_min_length || value > random_bytes_max_length) {
        mylite_random_bytes_set_length_out_of_range_error(database);
        return MYLITE_ERROR;
    }
    *out_length = (size_t)value;
    return MYLITE_OK;
}

int mylite_random_bytes_length_from_double(
    struct mylite_db *database,
    double value,
    size_t *out_length
) {
    return random_bytes_length_from_finite_double(database, value, out_length);
}

int mylite_random_bytes_length_from_text(
    struct mylite_db *database,
    const void *input,
    size_t input_size,
    size_t *out_length
) {
    char *text = NULL;
    char *parse_end = NULL;
    double value = 0.0;
    int rc = MYLITE_OK;

    if ((input == NULL && input_size != 0U) || out_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_length = 0U;

    text = (char *)malloc(input_size + 1U);
    if (text == NULL) {
        return MYLITE_NOMEM;
    }
    if (input_size != 0U) {
        memcpy(text, input, input_size);
    }
    text[input_size] = '\0';

    value = strtod(text, &parse_end);
    if (parse_end == text) {
        free(text);
        mylite_random_bytes_set_length_out_of_range_error(database);
        return MYLITE_ERROR;
    }
    if (input_has_nonspace_suffix(text, parse_end)) {
        rc = append_truncated_incorrect_integer_warning(database, input, input_size);
        if (rc != MYLITE_OK) {
            free(text);
            return rc;
        }
    }
    free(text);

    return random_bytes_length_from_finite_double(database, value, out_length);
}

void mylite_random_bytes_set_length_out_of_range_error(struct mylite_db *database) {
    if (database == NULL) {
        return;
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_random_bytes_length_out_of_range,
        "22003",
        "length value is out of range in 'random_bytes'"
    );
}

int mylite_sqlite_register_random_bytes_function(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_random_bytes",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = random_bytes_sqlite_callback,
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

static void random_bytes_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    unsigned char *bytes = NULL;
    size_t length = 0U;
    bool is_null = false;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite RANDOM_BYTES callback", -1);
        return;
    }

    rc = random_bytes_length_from_sqlite_value(context, argv[0], &length, &is_null);
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK) {
        sqlite3_result_error(context, "length value is out of range in 'random_bytes'", -1);
        return;
    }
    if (is_null) {
        sqlite3_result_null(context);
        return;
    }

    rc = mylite_random_bytes_generate(length, &bytes);
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK || length > (size_t)INT_MAX) {
        free(bytes);
        sqlite3_result_error(context, "MyLite RANDOM_BYTES generation failed", -1);
        return;
    }

    sqlite3_result_blob(context, bytes, (int)length, SQLITE_TRANSIENT);
    free(bytes);
}

static int random_bytes_length_from_sqlite_value(
    sqlite3_context *context,
    sqlite3_value *value,
    size_t *out_length,
    bool *out_is_null
) {
    struct mylite_db *database = NULL;
    int value_type = SQLITE_NULL;

    if (context == NULL || value == NULL || out_length == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }
    *out_length = 0U;
    *out_is_null = false;

    database = mylite_sqlite_bootstrap_owner_from_context(context);
    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite RANDOM_BYTES owner", -1);
        return MYLITE_ERROR;
    }

    value_type = sqlite3_value_type(value);
    if (value_type == SQLITE_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (value_type == SQLITE_INTEGER) {
        return mylite_random_bytes_length_from_int64(
            database,
            (int64_t)sqlite3_value_int64(value),
            out_length
        );
    }
    if (value_type == SQLITE_FLOAT) {
        return mylite_random_bytes_length_from_double(
            database,
            sqlite3_value_double(value),
            out_length
        );
    }
    if (value_type == SQLITE_TEXT || value_type == SQLITE_BLOB) {
        const void *bytes = sqlite3_value_blob(value);
        int byte_count = sqlite3_value_bytes(value);

        if ((bytes == NULL && byte_count != 0) || byte_count < 0) {
            return MYLITE_NOMEM;
        }
        return mylite_random_bytes_length_from_text(
            database,
            bytes,
            (size_t)byte_count,
            out_length
        );
    }

    mylite_random_bytes_set_length_out_of_range_error(database);
    return MYLITE_ERROR;
}

static int random_bytes_length_from_finite_double(
    struct mylite_db *database,
    double value,
    size_t *out_length
) {
    double rounded = 0.0;

    if (out_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_length = 0U;
    if (!isfinite(value)) {
        mylite_random_bytes_set_length_out_of_range_error(database);
        return MYLITE_ERROR;
    }

    rounded = floor(value + random_bytes_round_half_increment);
    if (rounded < (double)random_bytes_min_length || rounded > (double)random_bytes_max_length) {
        mylite_random_bytes_set_length_out_of_range_error(database);
        return MYLITE_ERROR;
    }

    *out_length = (size_t)rounded;
    return MYLITE_OK;
}

static int append_truncated_incorrect_integer_warning(
    struct mylite_db *database,
    const void *input,
    size_t input_size
) {
    char value_text[random_bytes_warning_input_capacity];
    char message
        [sizeof("Truncated incorrect INTEGER value: ''") + random_bytes_warning_input_capacity];
    int written = 0;
    int rc = MYLITE_OK;

    if (database == NULL || (input == NULL && input_size != 0U)) {
        return MYLITE_MISUSE;
    }

    format_warning_input(input, input_size, value_text);
    written =
        snprintf(message, sizeof(message), "Truncated incorrect INTEGER value: '%s'", value_text);
    if (written < 0 || (size_t)written >= sizeof(message)) {
        return MYLITE_ERROR;
    }

    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_truncated_incorrect_integer,
        "22007",
        message
    );
    if (rc == MYLITE_NOMEM) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_NOMEM,
            "HY001",
            "out of memory while recording RANDOM_BYTES() warning"
        );
    }
    return rc;
}

static void format_warning_input(
    const void *input,
    size_t input_size,
    char destination[random_bytes_warning_input_capacity]
) {
    const unsigned char *bytes = input;
    size_t limit = input_size;

    if (destination == NULL) {
        return;
    }
    if (bytes == NULL) {
        destination[0] = '\0';
        return;
    }

    if (limit > random_bytes_warning_input_capacity - 1U) {
        limit = random_bytes_warning_input_capacity - 1U;
    }
    for (size_t index = 0U; index < limit; ++index) {
        destination[index] = warning_input_printable_byte(bytes[index]);
    }
    destination[limit] = '\0';
}

static char warning_input_printable_byte(unsigned char byte) {
    static const char printable_ascii[] =
        " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
        "abcdefghijklmnopqrstuvwxyz{|}~";

    if (!warning_input_byte_is_printable(byte)) {
        return '?';
    }
    return printable_ascii[(size_t)byte - random_bytes_printable_ascii_min];
}

static bool input_has_nonspace_suffix(const char *start, const char *end) {
    if (start == NULL || end == NULL) {
        return false;
    }
    while (*end != '\0') {
        if (!isspace((unsigned char)*end)) {
            return true;
        }
        ++end;
    }
    return false;
}

static int fill_random_bytes(unsigned char *bytes, size_t length) {
    if (bytes == NULL && length != 0U) {
        return MYLITE_MISUSE;
    }

#ifdef _WIN32
    return fill_random_bytes_windows(bytes, length);
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || \
    defined(__DragonFly__)
    return fill_random_bytes_arc4random(bytes, length);
#elif defined(__linux__)
    {
        bool unavailable = false;
        int rc = fill_random_bytes_getrandom(bytes, length, &unavailable);

        if (rc == MYLITE_OK || !unavailable) {
            return rc;
        }
    }
    return fill_random_bytes_from_urandom(bytes, length);
#else
    return fill_random_bytes_from_urandom(bytes, length);
#endif
}

#ifdef _WIN32
static int fill_random_bytes_windows(unsigned char *bytes, size_t length) {
    if (length > (size_t)ULONG_MAX) {
        return MYLITE_MISUSE;
    }
    if (BCryptGenRandom(NULL, bytes, (ULONG)length, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}
#else
#  if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || \
      defined(__DragonFly__)
static int fill_random_bytes_arc4random(unsigned char *bytes, size_t length) {
    arc4random_buf(bytes, length);
    return MYLITE_OK;
}
#  endif

#  ifdef __linux__
static int fill_random_bytes_getrandom(unsigned char *bytes, size_t length, bool *out_unavailable) {
    size_t offset = 0U;

    if (out_unavailable == NULL) {
        return MYLITE_MISUSE;
    }
    *out_unavailable = false;

    while (offset < length) {
        ssize_t count = getrandom(bytes + offset, length - offset, 0);

        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == ENOSYS) {
                *out_unavailable = true;
            }
            return MYLITE_ERROR;
        }
        if (count == 0) {
            return MYLITE_ERROR;
        }
        offset += (size_t)count;
    }
    return MYLITE_OK;
}
#  endif

#  ifdef MYLITE_RANDOM_BYTES_HAS_URANDOM_FALLBACK
static int fill_random_bytes_from_urandom(unsigned char *bytes, size_t length) {
    size_t offset = 0U;
    int descriptor = open("/dev/urandom", O_RDONLY);

    if (descriptor < 0) {
        return MYLITE_ERROR;
    }
    while (offset < length) {
        ssize_t count = read(descriptor, bytes + offset, length - offset);

        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(descriptor);
            return MYLITE_ERROR;
        }
        if (count == 0) {
            close(descriptor);
            return MYLITE_ERROR;
        }
        offset += (size_t)count;
    }
    close(descriptor);
    return MYLITE_OK;
}
#  endif
#endif

static bool warning_input_byte_is_printable(unsigned char byte) {
    return byte >= random_bytes_printable_ascii_min && byte <= random_bytes_printable_ascii_max;
}
