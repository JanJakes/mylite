#include "mylite_spatial_collect_aggregate.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_spatial.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <mylite/mylite.h>

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    mysql_error_invalid_gis_data = 3037,
    mysql_error_different_srids = 4034,
    spatial_collect_initial_capacity = 128,
    spatial_collect_srid_error_message_capacity = 160,
    spatial_collect_internal_srid_size = 4,
    spatial_collect_wkb_header_size = 9,
    spatial_collect_wkb_type_offset = 1,
    spatial_collect_wkb_collection_count_offset = 5,
    spatial_collect_wkb_payload_offset = 4,
    spatial_collect_wkb_little_endian = 1,
};

struct spatial_collect_config {
    bool is_distinct;
};

struct spatial_collect_distinct_value {
    unsigned char *bytes;
    size_t size;
    struct spatial_collect_distinct_value *next;
};

struct spatial_collect_state {
    unsigned char *payload;
    size_t payload_size;
    size_t payload_capacity;
    struct spatial_collect_distinct_value *distinct_values;
    struct spatial_collect_distinct_value *last_distinct_value;
    enum mylite_spatial_geometry_type result_type;
    uint32_t srid;
    uint32_t geometry_count;
    bool saw_value;
    bool failed;
};

static void spatial_collect_step(sqlite3_context *context, int argc, sqlite3_value **argv);
static void spatial_collect_final(sqlite3_context *context);
static int spatial_collect_append_value(
    sqlite3_context *context,
    struct spatial_collect_state *state,
    sqlite3_value *value,
    const struct spatial_collect_config *config
);
static int spatial_collect_record_distinct_value(
    struct spatial_collect_state *state,
    const unsigned char *bytes,
    size_t byte_count,
    bool *out_duplicate
);
static int spatial_collect_append_payload(
    struct spatial_collect_state *state,
    const unsigned char *bytes,
    size_t byte_count
);
static int spatial_collect_reserve(struct spatial_collect_state *state, size_t needed);
static void spatial_collect_update_result_type(
    struct spatial_collect_state *state,
    enum mylite_spatial_geometry_type input_type
);
static enum mylite_spatial_geometry_type spatial_collect_type_for_first_input(
    enum mylite_spatial_geometry_type input_type
);
static enum mylite_spatial_geometry_type spatial_collect_expected_input_type(
    enum mylite_spatial_geometry_type result_type
);
static void spatial_collect_set_invalid_geometry_error(sqlite3_context *context);
static void spatial_collect_set_mixed_srid_error(
    sqlite3_context *context,
    uint32_t first_srid,
    uint32_t second_srid
);
static void spatial_collect_set_runtime_error(sqlite3_context *context, const char *message);
static void spatial_collect_set_diagnostic_error(
    sqlite3_context *context,
    int code,
    const char *sqlstate,
    const char *message
);
static int spatial_collect_result_bytes(
    const struct spatial_collect_state *state,
    unsigned char **out_bytes,
    size_t *out_size
);
static void spatial_collect_write_u32(unsigned char *destination, uint32_t value);
static void spatial_collect_state_deinit(struct spatial_collect_state *state);

int mylite_sqlite_register_spatial_collect_aggregate_function(sqlite3 *sqlite) {
    static struct spatial_collect_config plain_config = {.is_distinct = false};
    static struct spatial_collect_config distinct_config = {.is_distinct = true};
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_AGGREGATE,
            .name = "_mylite_st_collect",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = &plain_config,
            .scalar_callback = NULL,
            .step_callback = spatial_collect_step,
            .final_callback = spatial_collect_final,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_AGGREGATE,
            .name = "_mylite_st_collect_distinct",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = &distinct_config,
            .scalar_callback = NULL,
            .step_callback = spatial_collect_step,
            .final_callback = spatial_collect_final,
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

static void spatial_collect_step(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const struct spatial_collect_config *config = sqlite3_user_data(context);
    struct spatial_collect_state *state = NULL;
    int rc = MYLITE_OK;

    if (context == NULL) {
        return;
    }
    if (argc != 1 || argv == NULL || argv[0] == NULL || config == NULL) {
        sqlite3_result_error(context, "invalid MyLite ST_Collect callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        return;
    }

    state = sqlite3_aggregate_context(context, (int)sizeof(*state));
    if (state == NULL) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (state->failed) {
        return;
    }

    rc = spatial_collect_append_value(context, state, argv[0], config);
    if (rc == MYLITE_NOMEM) {
        state->failed = true;
        sqlite3_result_error_nomem(context);
    } else if (rc != MYLITE_OK) {
        state->failed = true;
    }
}

static void spatial_collect_final(sqlite3_context *context) {
    struct spatial_collect_state *state = NULL;
    unsigned char *bytes = NULL;
    size_t byte_count = 0U;
    int rc = MYLITE_OK;

    if (context == NULL) {
        return;
    }

    state = sqlite3_aggregate_context(context, 0);
    if (state == NULL || !state->saw_value) {
        sqlite3_result_null(context);
        return;
    }
    if (state->failed) {
        spatial_collect_state_deinit(state);
        return;
    }

    rc = spatial_collect_result_bytes(state, &bytes, &byte_count);
    if (rc == MYLITE_NOMEM) {
        spatial_collect_state_deinit(state);
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK || byte_count > (size_t)INT_MAX) {
        spatial_collect_state_deinit(state);
        free(bytes);
        spatial_collect_set_runtime_error(context, "MyLite ST_Collect result is too large");
        return;
    }

    sqlite3_result_blob(context, bytes, (int)byte_count, SQLITE_TRANSIENT);
    free(bytes);
    spatial_collect_state_deinit(state);
}

static int spatial_collect_append_value(
    sqlite3_context *context,
    struct spatial_collect_state *state,
    sqlite3_value *value,
    const struct spatial_collect_config *config
) {
    const unsigned char *bytes = sqlite3_value_blob(value);
    int sqlite_byte_count = sqlite3_value_bytes(value);
    size_t byte_count = 0U;
    enum mylite_spatial_geometry_type input_type = MYLITE_SPATIAL_GEOMETRY_NONE;
    uint32_t srid = 0U;
    bool duplicate = false;
    int rc = MYLITE_OK;

    if (bytes == NULL || sqlite_byte_count < 0) {
        spatial_collect_set_invalid_geometry_error(context);
        return MYLITE_ERROR;
    }
    byte_count = (size_t)sqlite_byte_count;
    if (!mylite_spatial_geometry_bytes_are_valid(bytes, byte_count)) {
        spatial_collect_set_invalid_geometry_error(context);
        return MYLITE_ERROR;
    }

    input_type = mylite_spatial_geometry_bytes_type(bytes, byte_count);
    srid = mylite_spatial_geometry_bytes_srid(bytes, byte_count);
    if (state->saw_value && state->srid != srid) {
        spatial_collect_set_mixed_srid_error(context, state->srid, srid);
        return MYLITE_ERROR;
    }
    if (config->is_distinct) {
        rc = spatial_collect_record_distinct_value(state, bytes, byte_count, &duplicate);
        if (rc != MYLITE_OK || duplicate) {
            return rc;
        }
    }
    if (state->geometry_count == UINT32_MAX || byte_count < spatial_collect_wkb_payload_offset) {
        spatial_collect_set_runtime_error(context, "MyLite ST_Collect result is too large");
        return MYLITE_ERROR;
    }

    if (!state->saw_value) {
        state->saw_value = true;
        state->srid = srid;
        state->result_type = spatial_collect_type_for_first_input(input_type);
    } else {
        spatial_collect_update_result_type(state, input_type);
    }

    rc = spatial_collect_append_payload(
        state,
        bytes + spatial_collect_wkb_payload_offset,
        byte_count - spatial_collect_wkb_payload_offset
    );
    if (rc == MYLITE_OK) {
        ++state->geometry_count;
    }
    return rc;
}

static int spatial_collect_record_distinct_value(
    struct spatial_collect_state *state,
    const unsigned char *bytes,
    size_t byte_count,
    bool *out_duplicate
) {
    struct spatial_collect_distinct_value *item = NULL;

    *out_duplicate = false;
    for (item = state->distinct_values; item != NULL; item = item->next) {
        if (item->size == byte_count && memcmp(item->bytes, bytes, byte_count) == 0) {
            *out_duplicate = true;
            return MYLITE_OK;
        }
    }

    item = calloc(1U, sizeof(*item));
    if (item == NULL) {
        return MYLITE_NOMEM;
    }
    item->bytes = malloc(byte_count);
    if (item->bytes == NULL) {
        free(item);
        return MYLITE_NOMEM;
    }
    memcpy(item->bytes, bytes, byte_count);
    item->size = byte_count;

    if (state->last_distinct_value == NULL) {
        state->distinct_values = item;
    } else {
        state->last_distinct_value->next = item;
    }
    state->last_distinct_value = item;
    return MYLITE_OK;
}

static int spatial_collect_append_payload(
    struct spatial_collect_state *state,
    const unsigned char *bytes,
    size_t byte_count
) {
    int rc = MYLITE_OK;

    if (byte_count > SIZE_MAX - state->payload_size) {
        return MYLITE_NOMEM;
    }
    rc = spatial_collect_reserve(state, state->payload_size + byte_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    memcpy(state->payload + state->payload_size, bytes, byte_count);
    state->payload_size += byte_count;
    return MYLITE_OK;
}

static int spatial_collect_reserve(struct spatial_collect_state *state, size_t needed) {
    unsigned char *bytes = NULL;
    size_t capacity = state->payload_capacity;

    if (needed <= state->payload_capacity) {
        return MYLITE_OK;
    }
    if (capacity == 0U) {
        capacity = spatial_collect_initial_capacity;
    }
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2U) {
            capacity = needed;
            break;
        }
        capacity *= 2U;
    }

    bytes = realloc(state->payload, capacity);
    if (bytes == NULL) {
        return MYLITE_NOMEM;
    }
    state->payload = bytes;
    state->payload_capacity = capacity;
    return MYLITE_OK;
}

static void spatial_collect_update_result_type(
    struct spatial_collect_state *state,
    enum mylite_spatial_geometry_type input_type
) {
    enum mylite_spatial_geometry_type expected_type =
        spatial_collect_expected_input_type(state->result_type);

    if (expected_type == MYLITE_SPATIAL_GEOMETRY_NONE || input_type != expected_type) {
        state->result_type = MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION;
    }
}

static enum mylite_spatial_geometry_type spatial_collect_type_for_first_input(
    enum mylite_spatial_geometry_type input_type
) {
    switch (input_type) {
    case MYLITE_SPATIAL_GEOMETRY_POINT:
        return MYLITE_SPATIAL_GEOMETRY_MULTIPOINT;
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
        return MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING;
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
        return MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON;
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
    case MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION:
    case MYLITE_SPATIAL_GEOMETRY_NONE:
        return MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION;
    }

    return MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION;
}

static enum mylite_spatial_geometry_type spatial_collect_expected_input_type(
    enum mylite_spatial_geometry_type result_type
) {
    switch (result_type) {
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOINT:
        return MYLITE_SPATIAL_GEOMETRY_POINT;
    case MYLITE_SPATIAL_GEOMETRY_MULTILINESTRING:
        return MYLITE_SPATIAL_GEOMETRY_LINESTRING;
    case MYLITE_SPATIAL_GEOMETRY_MULTIPOLYGON:
        return MYLITE_SPATIAL_GEOMETRY_POLYGON;
    case MYLITE_SPATIAL_GEOMETRY_NONE:
    case MYLITE_SPATIAL_GEOMETRY_POINT:
    case MYLITE_SPATIAL_GEOMETRY_LINESTRING:
    case MYLITE_SPATIAL_GEOMETRY_POLYGON:
    case MYLITE_SPATIAL_GEOMETRY_GEOMETRYCOLLECTION:
        return MYLITE_SPATIAL_GEOMETRY_NONE;
    }

    return MYLITE_SPATIAL_GEOMETRY_NONE;
}

static void spatial_collect_set_invalid_geometry_error(sqlite3_context *context) {
    spatial_collect_set_diagnostic_error(
        context,
        mysql_error_invalid_gis_data,
        "22023",
        "Invalid GIS data provided to function st_collect."
    );
}

static void spatial_collect_set_mixed_srid_error(
    sqlite3_context *context,
    uint32_t first_srid,
    uint32_t second_srid
) {
    char message[spatial_collect_srid_error_message_capacity];
    int written = snprintf(
        message,
        sizeof(message),
        "Arguments to function st_collect contains geometries with different SRIDs: %u and %u. "
        "All geometries must have the same SRID.",
        first_srid,
        second_srid
    );

    if (written < 0 || (size_t)written >= sizeof(message)) {
        spatial_collect_set_runtime_error(context, "MyLite ST_Collect SRID error");
        return;
    }
    spatial_collect_set_diagnostic_error(context, mysql_error_different_srids, "22S05", message);
}

static void spatial_collect_set_runtime_error(sqlite3_context *context, const char *message) {
    spatial_collect_set_diagnostic_error(context, MYLITE_ERROR, "HY000", message);
}

static void spatial_collect_set_diagnostic_error(
    sqlite3_context *context,
    int code,
    const char *sqlstate,
    const char *message
) {
    struct mylite_db *database = mylite_sqlite_bootstrap_owner_from_context(context);

    if (database != NULL) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            code,
            sqlstate,
            message
        );
    }
    sqlite3_result_error(context, message, -1);
}

static int spatial_collect_result_bytes(
    const struct spatial_collect_state *state,
    unsigned char **out_bytes,
    size_t *out_size
) {
    unsigned char *bytes = NULL;
    size_t byte_count = 0U;

    *out_bytes = NULL;
    *out_size = 0U;
    if (state->payload_size >
        SIZE_MAX - spatial_collect_internal_srid_size - spatial_collect_wkb_header_size) {
        return MYLITE_NOMEM;
    }

    byte_count =
        spatial_collect_internal_srid_size + spatial_collect_wkb_header_size + state->payload_size;
    bytes = malloc(byte_count);
    if (bytes == NULL) {
        return MYLITE_NOMEM;
    }

    spatial_collect_write_u32(bytes, state->srid);
    bytes[spatial_collect_internal_srid_size] = spatial_collect_wkb_little_endian;
    spatial_collect_write_u32(
        bytes + spatial_collect_internal_srid_size + spatial_collect_wkb_type_offset,
        state->result_type
    );
    spatial_collect_write_u32(
        bytes + spatial_collect_internal_srid_size + spatial_collect_wkb_collection_count_offset,
        state->geometry_count
    );
    memcpy(
        bytes + spatial_collect_internal_srid_size + spatial_collect_wkb_header_size,
        state->payload,
        state->payload_size
    );

    *out_bytes = bytes;
    *out_size = byte_count;
    return MYLITE_OK;
}

static void spatial_collect_write_u32(unsigned char *destination, uint32_t value) {
    for (size_t i = 0U; i < spatial_collect_internal_srid_size; ++i) {
        unsigned int shift = (unsigned int)(i * CHAR_BIT);

        destination[i] = (unsigned char)((value >> shift) & UCHAR_MAX);
    }
}

static void spatial_collect_state_deinit(struct spatial_collect_state *state) {
    struct spatial_collect_distinct_value *item = NULL;

    if (state == NULL) {
        return;
    }
    item = state->distinct_values;
    while (item != NULL) {
        struct spatial_collect_distinct_value *next = item->next;

        free(item->bytes);
        free(item);
        item = next;
    }
    free(state->payload);
    *state = (struct spatial_collect_state){0};
}
