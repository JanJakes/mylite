#include "runtime/mylite_spatial.h"

#include "mylite_fuzzer.h"

#include <mylite/mylite.h>

#include <stdlib.h>
#include <string.h>

static const unsigned char valid_point[] = {
    0x00U, 0x00U, 0x00U, 0x00U, 0x01U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0xf0U, 0x3fU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x40U,
};

enum {
    generated_wkb_wrapper_modulo = 55,
    generated_wkb_default_wrapper_count = 50,
    generated_wkb_big_endian_flag = 1,
    generated_wkb_alternate_endian_flag = 2,
    generated_wkb_empty_flag = 4,
    generated_wkb_truncate_flag = 8,
    wkb_collection_header_size = 9,
    wkb_collection_count_offset = 5,
    wkb_point_size = 21,
    hex_low_nibble_mask = 0x0f,
    spatial_byte_bit_count = 8,
    spatial_byte_mask = 0xff,
};

static void fuzz_spatial_constructor(
    enum mylite_spatial_function_kind kind,
    const void *bytes,
    size_t byte_count
);
static void fuzz_generated_wkb(const uint8_t *data, size_t size);
static void fuzz_sql_wkb_paths(const uint8_t *data, size_t size);
static void execute_wkb_sql(mylite_db *database, const uint8_t *data, size_t size);
static char *make_wkb_sql(const uint8_t *data, size_t size);
static unsigned char *make_nested_wkb(size_t wrapper_count, unsigned char flags, size_t *out_size);
static void write_u32(unsigned char *destination, uint32_t value, bool little_endian);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    unsigned char near_valid_point[sizeof(valid_point)];
    size_t overlay_size = size < sizeof(near_valid_point) ? size : sizeof(near_valid_point);

    (void)mylite_spatial_geometry_bytes_are_valid(data, size);
    (void)mylite_spatial_geometry_bytes_type(data, size);
    (void)mylite_spatial_geometry_bytes_srid(data, size);

    memcpy(near_valid_point, valid_point, sizeof(near_valid_point));
    if (overlay_size > 0U) {
        memcpy(near_valid_point, data, overlay_size);
    }
    (void)mylite_spatial_geometry_bytes_are_valid(near_valid_point, sizeof(near_valid_point));
    (void)mylite_spatial_geometry_bytes_type(near_valid_point, sizeof(near_valid_point));
    (void)mylite_spatial_geometry_bytes_srid(near_valid_point, sizeof(near_valid_point));
    fuzz_spatial_constructor(MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMTEXT, data, size);
    fuzz_spatial_constructor(MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMWKB, data, size);
    if (size > 0U && data[0] == (uint8_t)'S') {
        fuzz_sql_wkb_paths(data + 1U, size - 1U);
    }
    if (size > 0U && data[0] == (uint8_t)'N') {
        fuzz_generated_wkb(data, size);
    }
    return 0;
}

static void fuzz_spatial_constructor(
    enum mylite_spatial_function_kind kind,
    const void *bytes,
    size_t byte_count
) {
    struct mylite_spatial_argument argument = {.bytes = bytes, .byte_count = byte_count};
    struct mylite_spatial_result result = {0};
    struct mylite_spatial_error error = {0};

    (void)mylite_spatial_evaluate(kind, &argument, 1U, &result, &error);
    mylite_spatial_result_deinit(&result);
}

static void fuzz_generated_wkb(const uint8_t *data, size_t size) {
    size_t wrapper_count = size > 1U ? (size_t)(data[1] % generated_wkb_wrapper_modulo)
                                     : generated_wkb_default_wrapper_count;
    unsigned char flags = size > 2U ? data[2] : generated_wkb_alternate_endian_flag;
    size_t wkb_size = 0U;
    unsigned char *wkb = make_nested_wkb(wrapper_count, flags, &wkb_size);

    if (wkb == NULL) {
        return;
    }
    fuzz_spatial_constructor(MYLITE_SPATIAL_FUNCTION_ST_GEOMFROMWKB, wkb, wkb_size);
    fuzz_sql_wkb_paths(wkb, wkb_size);
    free(wkb);
}

static void fuzz_sql_wkb_paths(const uint8_t *data, size_t size) {
    unsigned char near_valid_wkb[sizeof(valid_point) - 4U];
    size_t overlay_size = size < sizeof(near_valid_wkb) ? size : sizeof(near_valid_wkb);
    mylite_db *database = NULL;

    memcpy(near_valid_wkb, valid_point + 4U, sizeof(near_valid_wkb));
    memcpy(near_valid_wkb, data, overlay_size);
    if (mylite_open_memory(&database) == MYLITE_OK) {
        execute_wkb_sql(database, data, size);
        execute_wkb_sql(database, near_valid_wkb, sizeof(near_valid_wkb));
    }
    mylite_close(database);
}

static void execute_wkb_sql(mylite_db *database, const uint8_t *data, size_t size) {
    char *sql = make_wkb_sql(data, size);
    mylite_result *result = NULL;

    if (sql == NULL) {
        return;
    }
    (void)mylite_execute(database, sql, strlen(sql), &result);
    mylite_result_free(result);
    free(sql);
}

static char *make_wkb_sql(const uint8_t *data, size_t size) {
    static const char prefix[] = "SELECT ST_AsWKB(ST_GeomFromWKB(X'";
    static const char suffix[] = "'))";
    static const char hex_digits[] = "0123456789abcdef";
    const size_t fixed_size = sizeof(prefix) - 1U + sizeof(suffix);
    char *sql = NULL;
    size_t offset = sizeof(prefix) - 1U;

    if (size > (SIZE_MAX - fixed_size) / 2U) {
        return NULL;
    }
    sql = malloc(fixed_size + (size * 2U));
    if (sql == NULL) {
        return NULL;
    }
    memcpy(sql, prefix, sizeof(prefix) - 1U);
    for (size_t index = 0U; index < size; ++index) {
        sql[offset++] = hex_digits[data[index] >> 4U];
        sql[offset++] = hex_digits[data[index] & hex_low_nibble_mask];
    }
    memcpy(sql + offset, suffix, sizeof(suffix));
    return sql;
}

static unsigned char *make_nested_wkb(size_t wrapper_count, unsigned char flags, size_t *out_size) {
    bool terminal_is_empty = (flags & generated_wkb_empty_flag) != 0U;
    bool truncate_terminal = (flags & generated_wkb_truncate_flag) != 0U;
    size_t terminal_size = terminal_is_empty ? wkb_collection_header_size : wkb_point_size;
    size_t size = terminal_size + (wrapper_count * wkb_collection_header_size);
    unsigned char *wkb = calloc(size, 1U);
    size_t offset = 0U;

    if (wkb == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < wrapper_count; ++index) {
        bool little_endian = (flags & generated_wkb_alternate_endian_flag) != 0U
                                 ? ((index % 2U) == 0U)
                                 : (flags & generated_wkb_big_endian_flag) == 0U;

        wkb[offset] = little_endian ? 1U : 0U;
        write_u32(wkb + offset + 1U, MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION, little_endian);
        write_u32(wkb + offset + wkb_collection_count_offset, 1U, little_endian);
        offset += wkb_collection_header_size;
    }
    {
        bool little_endian = (flags & generated_wkb_alternate_endian_flag) != 0U
                                 ? ((wrapper_count % 2U) == 0U)
                                 : (flags & generated_wkb_big_endian_flag) == 0U;
        uint32_t type = terminal_is_empty ? MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION
                                          : MYLITE_SPATIAL_GEOMETRY_POINT;

        wkb[offset] = little_endian ? 1U : 0U;
        write_u32(wkb + offset + 1U, type, little_endian);
        if (terminal_is_empty) {
            write_u32(wkb + offset + wkb_collection_count_offset, 0U, little_endian);
        }
    }
    if (truncate_terminal) {
        size = offset + wkb_collection_count_offset;
    }
    *out_size = size;
    return wkb;
}

static void write_u32(unsigned char *destination, uint32_t value, bool little_endian) {
    for (size_t index = 0U; index < sizeof(value); ++index) {
        size_t destination_index = little_endian ? index : sizeof(value) - index - 1U;

        destination[destination_index] =
            (unsigned char)((value >> (index * spatial_byte_bit_count)) & spatial_byte_mask);
    }
}
