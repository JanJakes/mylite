#include "mylite_string_compression.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <mylite/mylite.h>

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

enum {
    mysql_warning_uncompress_too_large = 1256,
    mysql_warning_zlib = 1259,
    mysql_compress_length_prefix_size = 4,
    mysql_compress_length_mask = 0x3fffffffU,
    mysql_uncompress_max_output_size = 67108864,
    compression_decimal_text_capacity = 32,
    compression_result_too_large = 1,
    compression_inflate_buffer_size = 8192,
    compression_le32_second_byte_shift = 8,
    compression_le32_third_byte_shift = 16,
    compression_le32_fourth_byte_shift = 24,
};

struct compressed_payload_validation_request {
    const unsigned char *payload;
    size_t payload_size;
    uint32_t expected_size;
};

static void compress_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void uncompress_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void uncompressed_length_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static int sqlite_value_bytes_for_compression(
    sqlite3_context *context,
    sqlite3_value *value,
    char *integer_text,
    size_t integer_text_size,
    const void **out_bytes,
    size_t *out_size,
    bool *out_is_null
);
static void compress_sqlite_result(sqlite3_context *context, const void *input, size_t input_size);
static void uncompress_sqlite_result(
    sqlite3_context *context,
    const void *input,
    size_t input_size
);
static void uncompressed_length_sqlite_result(
    sqlite3_context *context,
    const void *input,
    size_t input_size
);
static int alloc_empty_bytes(unsigned char **out_bytes);
static int validate_compressed_payload(
    struct compressed_payload_validation_request request,
    bool *out_valid
);
static bool compressed_payload_exceeds_uncompress_limit(const void *input, size_t input_size);
static uint32_t load_le32(const unsigned char bytes[mysql_compress_length_prefix_size]);
static void store_le32(unsigned char bytes[mysql_compress_length_prefix_size], uint32_t value);

int mylite_string_compress(
    const void *input,
    size_t input_size,
    unsigned char **out_bytes,
    size_t *out_size
) {
    const unsigned char *bytes = input;
    unsigned char *output = NULL;
    uLongf compressed_size = 0U;
    uLongf compressed_bound = 0U;
    int zlib_rc = Z_OK;

    if ((input == NULL && input_size != 0U) || out_bytes == NULL || out_size == NULL) {
        return MYLITE_MISUSE;
    }

    *out_bytes = NULL;
    *out_size = 0U;

    if (input_size == 0U) {
        return alloc_empty_bytes(out_bytes);
    }
    if (input_size > UINT32_MAX || input_size > (size_t)ULONG_MAX) {
        return MYLITE_ERROR;
    }

    compressed_bound = compressBound((uLong)input_size);
    if ((size_t)compressed_bound > SIZE_MAX - mysql_compress_length_prefix_size) {
        return MYLITE_NOMEM;
    }
    output = (unsigned char *)malloc((size_t)compressed_bound + mysql_compress_length_prefix_size);
    if (output == NULL) {
        return MYLITE_NOMEM;
    }

    compressed_size = compressed_bound;
    zlib_rc = compress2(
        output + mysql_compress_length_prefix_size,
        &compressed_size,
        bytes,
        (uLong)input_size,
        Z_DEFAULT_COMPRESSION
    );
    if (zlib_rc != Z_OK) {
        free(output);
        return MYLITE_ERROR;
    }

    store_le32(output, (uint32_t)input_size);
    *out_bytes = output;
    *out_size = (size_t)compressed_size + mysql_compress_length_prefix_size;
    return MYLITE_OK;
}

int mylite_string_uncompress(
    const void *input,
    size_t input_size,
    unsigned char **out_bytes,
    size_t *out_size,
    bool *out_valid
) {
    const unsigned char *bytes = input;
    unsigned char *output = NULL;
    uint32_t original_size = 0U;
    uLongf actual_size = 0U;
    bool stream_valid = false;
    int zlib_rc = Z_OK;
    int rc = MYLITE_OK;

    if ((input == NULL && input_size != 0U) || out_bytes == NULL || out_size == NULL ||
        out_valid == NULL) {
        return MYLITE_MISUSE;
    }

    *out_bytes = NULL;
    *out_size = 0U;
    *out_valid = false;

    if (input_size == 0U) {
        rc = alloc_empty_bytes(out_bytes);

        if (rc == MYLITE_OK) {
            *out_valid = true;
        }
        return rc;
    }
    if (input_size <= mysql_compress_length_prefix_size) {
        return MYLITE_OK;
    }

    original_size = load_le32(bytes) & mysql_compress_length_mask;
    if (original_size > mysql_uncompress_max_output_size) {
        return MYLITE_OK;
    }
    rc = validate_compressed_payload(
        (struct compressed_payload_validation_request){
            .payload = bytes + mysql_compress_length_prefix_size,
            .payload_size = input_size - mysql_compress_length_prefix_size,
            .expected_size = original_size,
        },
        &stream_valid
    );
    if (rc != MYLITE_OK || !stream_valid) {
        return rc;
    }
#if SIZE_MAX <= UINT32_MAX
    if ((size_t)original_size == SIZE_MAX) {
        return MYLITE_NOMEM;
    }
#endif
#if SIZE_MAX > ULONG_MAX
    if (input_size - mysql_compress_length_prefix_size > (size_t)ULONG_MAX) {
        return MYLITE_NOMEM;
    }
#endif

    output = (unsigned char *)malloc((size_t)original_size + 1U);
    if (output == NULL) {
        return MYLITE_NOMEM;
    }

    actual_size = original_size;
    zlib_rc = uncompress(
        output,
        &actual_size,
        bytes + mysql_compress_length_prefix_size,
        (uLong)(input_size - mysql_compress_length_prefix_size)
    );
    if (zlib_rc != Z_OK || actual_size != original_size) {
        free(output);
        return MYLITE_OK;
    }

    output[original_size] = '\0';
    *out_bytes = output;
    *out_size = original_size;
    *out_valid = true;
    return MYLITE_OK;
}

int mylite_string_uncompressed_length(
    const void *input,
    size_t input_size,
    uint32_t *out_length,
    bool *out_valid
) {
    const unsigned char *bytes = input;
    int rc = MYLITE_OK;

    if ((input == NULL && input_size != 0U) || out_length == NULL || out_valid == NULL) {
        return MYLITE_MISUSE;
    }

    *out_length = 0U;
    *out_valid = false;

    if (input_size == 0U) {
        *out_valid = true;
        return MYLITE_OK;
    }
    if (input_size <= mysql_compress_length_prefix_size) {
        return MYLITE_OK;
    }

    *out_length = load_le32(bytes) & mysql_compress_length_mask;
    *out_valid = true;
    return rc;
}

int mylite_string_compression_append_zlib_warning(struct mylite_db *database) {
    int rc = MYLITE_OK;

    if (database == NULL) {
        return MYLITE_MISUSE;
    }

    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_zlib,
        "HY000",
        "ZLIB: Input data corrupted"
    );
    if (rc == MYLITE_NOMEM) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_NOMEM,
            "HY001",
            "out of memory while recording zlib warning"
        );
    }
    return rc;
}

int mylite_string_compression_append_uncompress_warning(
    struct mylite_db *database,
    const void *input,
    size_t input_size
) {
    int rc = MYLITE_OK;

    if (database == NULL || (input == NULL && input_size != 0U)) {
        return MYLITE_MISUSE;
    }
    if (!compressed_payload_exceeds_uncompress_limit(input, input_size)) {
        return mylite_string_compression_append_zlib_warning(database);
    }

    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_uncompress_too_large,
        "HY000",
        "Uncompressed data size too large; the maximum size is 67108864 "
        "(probably, length of uncompressed data was corrupted)"
    );
    if (rc == MYLITE_NOMEM) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_NOMEM,
            "HY001",
            "out of memory while recording uncompress size warning"
        );
    }
    return rc;
}

int mylite_sqlite_register_string_compression_functions(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_compress",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = compress_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_uncompress",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = uncompress_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_uncompressed_length",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = uncompressed_length_sqlite_callback,
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

static void compress_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    char integer_text[compression_decimal_text_capacity];
    const void *bytes = NULL;
    size_t byte_count = 0U;
    bool is_null = false;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite COMPRESS callback", -1);
        return;
    }

    if (sqlite_value_bytes_for_compression(
            context,
            argv[0],
            integer_text,
            sizeof(integer_text),
            &bytes,
            &byte_count,
            &is_null
        ) != MYLITE_OK) {
        return;
    }
    if (is_null) {
        sqlite3_result_null(context);
        return;
    }

    compress_sqlite_result(context, bytes, byte_count);
}

static void uncompress_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    char integer_text[compression_decimal_text_capacity];
    const void *bytes = NULL;
    size_t byte_count = 0U;
    bool is_null = false;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite UNCOMPRESS callback", -1);
        return;
    }

    if (sqlite_value_bytes_for_compression(
            context,
            argv[0],
            integer_text,
            sizeof(integer_text),
            &bytes,
            &byte_count,
            &is_null
        ) != MYLITE_OK) {
        return;
    }
    if (is_null) {
        sqlite3_result_null(context);
        return;
    }

    uncompress_sqlite_result(context, bytes, byte_count);
}

static void uncompressed_length_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    char integer_text[compression_decimal_text_capacity];
    const void *bytes = NULL;
    size_t byte_count = 0U;
    bool is_null = false;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite UNCOMPRESSED_LENGTH callback", -1);
        return;
    }

    if (sqlite_value_bytes_for_compression(
            context,
            argv[0],
            integer_text,
            sizeof(integer_text),
            &bytes,
            &byte_count,
            &is_null
        ) != MYLITE_OK) {
        return;
    }
    if (is_null) {
        sqlite3_result_null(context);
        return;
    }

    uncompressed_length_sqlite_result(context, bytes, byte_count);
}

static int sqlite_value_bytes_for_compression(
    sqlite3_context *context,
    sqlite3_value *value,
    char *integer_text,
    size_t integer_text_size,
    const void **out_bytes,
    size_t *out_size,
    bool *out_is_null
) {
    int value_type = SQLITE_NULL;

    if (context == NULL || value == NULL || integer_text == NULL || integer_text_size == 0U ||
        out_bytes == NULL || out_size == NULL || out_is_null == NULL) {
        return MYLITE_MISUSE;
    }

    *out_bytes = NULL;
    *out_size = 0U;
    *out_is_null = false;

    value_type = sqlite3_value_type(value);
    if (value_type == SQLITE_NULL) {
        *out_is_null = true;
        return MYLITE_OK;
    }
    if (value_type == SQLITE_INTEGER) {
        int written = snprintf(
            integer_text,
            integer_text_size,
            "%" PRId64,
            (int64_t)sqlite3_value_int64(value)
        );

        if (written < 0 || (size_t)written >= integer_text_size) {
            sqlite3_result_error(context, "failed to format MyLite compression integer", -1);
            return MYLITE_ERROR;
        }
        *out_bytes = integer_text;
        *out_size = (size_t)written;
        return MYLITE_OK;
    }
    if (value_type == SQLITE_TEXT || value_type == SQLITE_BLOB) {
        const void *bytes = sqlite3_value_blob(value);
        int byte_count = sqlite3_value_bytes(value);

        if ((bytes == NULL && byte_count != 0) || byte_count < 0) {
            sqlite3_result_error_nomem(context);
            return MYLITE_NOMEM;
        }
        *out_bytes = bytes;
        *out_size = (size_t)byte_count;
        return MYLITE_OK;
    }

    sqlite3_result_null(context);
    *out_is_null = true;
    return MYLITE_OK;
}

static void compress_sqlite_result(sqlite3_context *context, const void *input, size_t input_size) {
    unsigned char *compressed = NULL;
    size_t compressed_size = 0U;
    int rc = mylite_string_compress(input, input_size, &compressed, &compressed_size);

    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK || compressed_size > (size_t)INT_MAX) {
        free(compressed);
        sqlite3_result_error(context, "MyLite COMPRESS result failed", -1);
        return;
    }

    sqlite3_result_blob(context, compressed, (int)compressed_size, SQLITE_TRANSIENT);
    free(compressed);
}

static void uncompress_sqlite_result(
    sqlite3_context *context,
    const void *input,
    size_t input_size
) {
    struct mylite_db *database = NULL;
    unsigned char *decoded = NULL;
    size_t decoded_size = 0U;
    bool valid = false;
    int rc = MYLITE_OK;

    database = mylite_sqlite_bootstrap_owner_from_context(context);
    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite UNCOMPRESS owner", -1);
        return;
    }

    rc = mylite_string_uncompress(input, input_size, &decoded, &decoded_size, &valid);
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK) {
        sqlite3_result_error(context, "MyLite UNCOMPRESS decode failed", -1);
        return;
    }
    if (!valid) {
        rc = mylite_string_compression_append_uncompress_warning(database, input, input_size);
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
            return;
        }
        if (rc != MYLITE_OK) {
            sqlite3_result_error(context, "MyLite UNCOMPRESS warning failed", -1);
            return;
        }
        sqlite3_result_null(context);
        return;
    }
    if (decoded_size > (size_t)INT_MAX) {
        free(decoded);
        sqlite3_result_error(context, "MyLite UNCOMPRESS result is too large", -1);
        return;
    }

    sqlite3_result_blob(context, decoded, (int)decoded_size, SQLITE_TRANSIENT);
    free(decoded);
}

static void uncompressed_length_sqlite_result(
    sqlite3_context *context,
    const void *input,
    size_t input_size
) {
    struct mylite_db *database = NULL;
    uint32_t original_size = 0U;
    bool valid = false;
    int rc = MYLITE_OK;

    database = mylite_sqlite_bootstrap_owner_from_context(context);
    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite UNCOMPRESSED_LENGTH owner", -1);
        return;
    }

    rc = mylite_string_uncompressed_length(input, input_size, &original_size, &valid);
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK) {
        sqlite3_result_error(context, "MyLite UNCOMPRESSED_LENGTH decode failed", -1);
        return;
    }
    if (!valid) {
        rc = mylite_string_compression_append_uncompress_warning(database, input, input_size);
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
            return;
        }
        if (rc != MYLITE_OK) {
            sqlite3_result_error(context, "MyLite UNCOMPRESSED_LENGTH warning failed", -1);
            return;
        }
        sqlite3_result_int(context, 0);
        return;
    }

    sqlite3_result_int64(context, (sqlite3_int64)original_size);
}

static int alloc_empty_bytes(unsigned char **out_bytes) {
    unsigned char *empty = NULL;

    if (out_bytes == NULL) {
        return MYLITE_MISUSE;
    }

    empty = (unsigned char *)malloc(1U);
    if (empty == NULL) {
        return MYLITE_NOMEM;
    }
    empty[0] = '\0';
    *out_bytes = empty;
    return MYLITE_OK;
}

static int validate_compressed_payload(
    struct compressed_payload_validation_request request,
    bool *out_valid
) {
    unsigned char buffer[compression_inflate_buffer_size];
    z_stream stream;
    size_t input_offset = 0U;
    size_t produced_total = 0U;
    int zlib_rc = Z_OK;

    if ((request.payload == NULL && request.payload_size != 0U) || out_valid == NULL) {
        return MYLITE_MISUSE;
    }

    *out_valid = false;
    stream = (z_stream){0};
    zlib_rc = inflateInit(&stream);
    if (zlib_rc != Z_OK) {
        return zlib_rc == Z_MEM_ERROR ? MYLITE_NOMEM : MYLITE_ERROR;
    }

    while (true) {
        size_t produced = 0U;

        if (stream.avail_in == 0U && input_offset < request.payload_size) {
            size_t remaining = request.payload_size - input_offset;
            uInt chunk_size = remaining > (size_t)UINT_MAX ? UINT_MAX : (uInt)remaining;

            stream.next_in = (Bytef *)(request.payload + input_offset);
            stream.avail_in = chunk_size;
            input_offset += chunk_size;
        }

        stream.next_out = buffer;
        stream.avail_out = (uInt)sizeof(buffer);
        zlib_rc = inflate(&stream, Z_NO_FLUSH);
        produced = sizeof(buffer) - stream.avail_out;
        if (produced > (size_t)request.expected_size - produced_total) {
            (void)inflateEnd(&stream);
            return MYLITE_OK;
        }
        produced_total += produced;

        if (zlib_rc == Z_STREAM_END) {
            *out_valid = produced_total == (size_t)request.expected_size;
            (void)inflateEnd(&stream);
            return MYLITE_OK;
        }
        if (zlib_rc != Z_OK) {
            (void)inflateEnd(&stream);
            return MYLITE_OK;
        }
    }
}

static bool compressed_payload_exceeds_uncompress_limit(const void *input, size_t input_size) {
    const unsigned char *bytes = input;
    uint32_t advertised_size = 0U;

    if (input_size <= mysql_compress_length_prefix_size) {
        return false;
    }

    advertised_size = load_le32(bytes) & mysql_compress_length_mask;
    return advertised_size > mysql_uncompress_max_output_size;
}

static uint32_t load_le32(const unsigned char bytes[mysql_compress_length_prefix_size]) {
    return ((uint32_t)bytes[0]) | ((uint32_t)bytes[1] << compression_le32_second_byte_shift) |
           ((uint32_t)bytes[2] << compression_le32_third_byte_shift) |
           ((uint32_t)bytes[3] << compression_le32_fourth_byte_shift);
}

static void store_le32(unsigned char bytes[mysql_compress_length_prefix_size], uint32_t value) {
    bytes[0] = (unsigned char)(value & UINT32_C(0xff));
    bytes[1] = (unsigned char)((value >> compression_le32_second_byte_shift) & UINT32_C(0xff));
    bytes[2] = (unsigned char)((value >> compression_le32_third_byte_shift) & UINT32_C(0xff));
    bytes[3] = (unsigned char)((value >> compression_le32_fourth_byte_shift) & UINT32_C(0xff));
}
