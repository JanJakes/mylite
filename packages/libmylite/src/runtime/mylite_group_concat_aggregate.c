#include "mylite_group_concat_aggregate.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    mysql_warning_group_concat_cut = 1260,
    group_concat_initial_capacity = 32,
    utf8_ascii_upper_bound = 0x80,
    utf8_two_byte_lead_min = 0xc2,
    utf8_two_byte_lead_max = 0xdf,
    utf8_three_byte_lead_min = 0xe0,
    utf8_three_byte_lead_max = 0xef,
    utf8_four_byte_lead_min = 0xf0,
    utf8_four_byte_lead_max = 0xf4,
    utf8_continuation_mask = 0xc0,
    utf8_continuation_tag = 0x80,
};

struct mylite_group_concat_config {
    bool is_distinct;
};

struct mylite_group_concat_distinct_value {
    char *bytes;
    size_t size;
    struct mylite_group_concat_distinct_value *next;
};

struct mylite_group_concat_state {
    char *bytes;
    size_t size;
    size_t capacity;
    struct mylite_group_concat_distinct_value *distinct_values;
    struct mylite_group_concat_distinct_value *last_distinct_value;
    uint64_t cut_ordinal;
    bool saw_value;
    bool truncated;
};

struct group_concat_append_request {
    const unsigned char *bytes;
    size_t byte_count;
    size_t limit;
};

static void group_concat_step(sqlite3_context *context, int argc, sqlite3_value **argv);
static void group_concat_final(sqlite3_context *context);
static int group_concat_append_value(
    sqlite3_context *context,
    struct mylite_db *database,
    struct mylite_group_concat_state *state,
    sqlite3_value *value,
    sqlite3_value *separator,
    const struct mylite_group_concat_config *config
);
static int group_concat_record_distinct_value(
    struct mylite_group_concat_state *state,
    const unsigned char *bytes,
    size_t byte_count,
    bool *out_duplicate
);
static int group_concat_append_bytes(
    struct mylite_group_concat_state *state,
    struct group_concat_append_request request,
    bool *out_complete
);
static size_t group_concat_size_limit(uint64_t value);
static int group_concat_reserve(struct mylite_group_concat_state *state, size_t needed);
static size_t group_concat_valid_utf8_prefix(const unsigned char *bytes, size_t byte_count);
static int group_concat_append_cut_warning(
    struct mylite_db *database,
    const struct mylite_group_concat_state *state
);
static void group_concat_state_deinit(struct mylite_group_concat_state *state);

int mylite_sqlite_register_group_concat_aggregate_function(sqlite3 *sqlite) {
    static struct mylite_group_concat_config plain_config = {.is_distinct = false};
    static struct mylite_group_concat_config distinct_config = {.is_distinct = true};
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_AGGREGATE,
            .name = "_mylite_group_concat",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = &plain_config,
            .scalar_callback = NULL,
            .step_callback = group_concat_step,
            .final_callback = group_concat_final,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_AGGREGATE,
            .name = "_mylite_group_concat",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = &plain_config,
            .scalar_callback = NULL,
            .step_callback = group_concat_step,
            .final_callback = group_concat_final,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_AGGREGATE,
            .name = "_mylite_group_concat_distinct",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = &distinct_config,
            .scalar_callback = NULL,
            .step_callback = group_concat_step,
            .final_callback = group_concat_final,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_AGGREGATE,
            .name = "_mylite_group_concat_distinct",
            .argument_count = 2,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = &distinct_config,
            .scalar_callback = NULL,
            .step_callback = group_concat_step,
            .final_callback = group_concat_final,
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

static void group_concat_step(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const struct mylite_group_concat_config *config = NULL;
    struct mylite_db *database = NULL;
    struct mylite_group_concat_state *state = NULL;
    sqlite3_value *separator = NULL;
    int value_type = SQLITE_NULL;
    int rc = MYLITE_OK;

    if (context == NULL) {
        return;
    }
    if ((argc != 1 && argc != 2) || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite GROUP_CONCAT callback", -1);
        return;
    }

    value_type = sqlite3_value_type(argv[0]);
    if (value_type == SQLITE_NULL) {
        return;
    }

    state = sqlite3_aggregate_context(context, (int)sizeof(*state));
    if (state == NULL) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (state->truncated) {
        return;
    }

    config = sqlite3_user_data(context);
    database = mylite_sqlite_bootstrap_owner_from_context(context);
    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite GROUP_CONCAT owner", -1);
        return;
    }
    if (database->session.group_concat_value_ordinal == UINT64_MAX) {
        sqlite3_result_error(context, "MyLite GROUP_CONCAT row ordinal overflow", -1);
        return;
    }
    ++database->session.group_concat_value_ordinal;

    if (argc == 2) {
        separator = argv[1];
    }
    rc = group_concat_append_value(context, database, state, argv[0], separator, config);
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
    } else if (rc != MYLITE_OK) {
        sqlite3_result_error(context, "MyLite GROUP_CONCAT append failed", -1);
    }
}

static void group_concat_final(sqlite3_context *context) {
    struct mylite_db *database = NULL;
    struct mylite_group_concat_state *state = NULL;
    int rc = MYLITE_OK;

    if (context == NULL) {
        return;
    }
    state = sqlite3_aggregate_context(context, 0);
    if (state == NULL || !state->saw_value) {
        sqlite3_result_null(context);
        return;
    }

    if (state->truncated) {
        database = mylite_sqlite_bootstrap_owner_from_context(context);
        if (database == NULL) {
            sqlite3_result_error(context, "missing MyLite GROUP_CONCAT owner", -1);
            group_concat_state_deinit(state);
            return;
        }
        rc = group_concat_append_cut_warning(database, state);
        if (rc == MYLITE_NOMEM) {
            sqlite3_result_error_nomem(context);
            group_concat_state_deinit(state);
            return;
        }
        if (rc != MYLITE_OK) {
            sqlite3_result_error(context, "MyLite GROUP_CONCAT warning failed", -1);
            group_concat_state_deinit(state);
            return;
        }
    }

    sqlite3_result_text64(
        context,
        state->bytes == NULL ? "" : state->bytes,
        (sqlite3_uint64)state->size,
        SQLITE_TRANSIENT,
        SQLITE_UTF8
    );
    group_concat_state_deinit(state);
}

static int group_concat_append_value(
    sqlite3_context *context,
    struct mylite_db *database,
    struct mylite_group_concat_state *state,
    sqlite3_value *value,
    sqlite3_value *separator,
    const struct mylite_group_concat_config *config
) {
    static const unsigned char default_separator[] = ",";

    const unsigned char *value_bytes = NULL;
    const unsigned char *separator_bytes = default_separator;
    int value_byte_count = 0;
    int separator_byte_count = (int)(sizeof(default_separator) - 1U);
    size_t limit = group_concat_size_limit(database->session.group_concat_max_len);
    uint64_t ordinal = database->session.group_concat_value_ordinal;
    bool complete = true;
    bool duplicate = false;
    int rc = MYLITE_OK;

    if (state == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }

    value_bytes = sqlite3_value_text(value);
    value_byte_count = sqlite3_value_bytes(value);
    if ((value_bytes == NULL && value_byte_count != 0) || value_byte_count < 0) {
        return MYLITE_NOMEM;
    }
    if (separator != NULL) {
        if (sqlite3_value_type(separator) == SQLITE_NULL) {
            sqlite3_result_error(context, "invalid MyLite GROUP_CONCAT separator", -1);
            return MYLITE_ERROR;
        }
        separator_bytes = sqlite3_value_text(separator);
        separator_byte_count = sqlite3_value_bytes(separator);
        if ((separator_bytes == NULL && separator_byte_count != 0) || separator_byte_count < 0) {
            return MYLITE_NOMEM;
        }
    }

    if (config != NULL && config->is_distinct) {
        rc = group_concat_record_distinct_value(
            state,
            value_bytes,
            (size_t)value_byte_count,
            &duplicate
        );
        if (rc != MYLITE_OK || duplicate) {
            return rc;
        }
    }

    if (state->saw_value) {
        rc = group_concat_append_bytes(
            state,
            (struct group_concat_append_request){
                .bytes = separator_bytes,
                .byte_count = (size_t)separator_byte_count,
                .limit = limit,
            },
            &complete
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (!complete) {
            state->truncated = true;
            state->cut_ordinal = ordinal;
            return MYLITE_OK;
        }
    }

    state->saw_value = true;
    rc = group_concat_append_bytes(
        state,
        (struct group_concat_append_request){
            .bytes = value_bytes,
            .byte_count = (size_t)value_byte_count,
            .limit = limit,
        },
        &complete
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!complete) {
        state->truncated = true;
        state->cut_ordinal = ordinal;
    }
    return MYLITE_OK;
}

static int group_concat_record_distinct_value(
    struct mylite_group_concat_state *state,
    const unsigned char *bytes,
    size_t byte_count,
    bool *out_duplicate
) {
    struct mylite_group_concat_distinct_value *current = NULL;
    struct mylite_group_concat_distinct_value *entry = NULL;
    size_t allocation_size = byte_count == 0U ? 1U : byte_count;

    if (state == NULL || out_duplicate == NULL || (bytes == NULL && byte_count != 0U)) {
        return MYLITE_MISUSE;
    }
    *out_duplicate = false;

    current = state->distinct_values;
    while (current != NULL) {
        if (current->size == byte_count &&
            (byte_count == 0U || memcmp(current->bytes, bytes, byte_count) == 0)) {
            *out_duplicate = true;
            return MYLITE_OK;
        }
        current = current->next;
    }

    entry = malloc(sizeof(*entry));
    if (entry == NULL) {
        return MYLITE_NOMEM;
    }
    *entry = (struct mylite_group_concat_distinct_value){0};
    entry->bytes = malloc(allocation_size);
    if (entry->bytes == NULL) {
        free(entry);
        return MYLITE_NOMEM;
    }
    if (byte_count != 0U) {
        memcpy(entry->bytes, bytes, byte_count);
    }
    entry->size = byte_count;

    if (state->last_distinct_value == NULL) {
        state->distinct_values = entry;
    } else {
        state->last_distinct_value->next = entry;
    }
    state->last_distinct_value = entry;
    return MYLITE_OK;
}

static int group_concat_append_bytes(
    struct mylite_group_concat_state *state,
    struct group_concat_append_request request,
    bool *out_complete
) {
    size_t available = 0U;
    size_t copy_count = 0U;
    int rc = MYLITE_OK;

    if (state == NULL || out_complete == NULL ||
        (request.bytes == NULL && request.byte_count != 0U)) {
        return MYLITE_MISUSE;
    }
    *out_complete = true;
    if (request.byte_count == 0U) {
        return MYLITE_OK;
    }
    if (state->size >= request.limit) {
        *out_complete = false;
        return MYLITE_OK;
    }

    available = request.limit - state->size;
    copy_count = request.byte_count <= available ? request.byte_count : available;
    copy_count = group_concat_valid_utf8_prefix(request.bytes, copy_count);
    if (copy_count > 0U) {
        if (copy_count > SIZE_MAX - state->size - 1U) {
            return MYLITE_NOMEM;
        }
        rc = group_concat_reserve(state, state->size + copy_count + 1U);
        if (rc != MYLITE_OK) {
            return rc;
        }
        memcpy(state->bytes + state->size, request.bytes, copy_count);
        state->size += copy_count;
        state->bytes[state->size] = '\0';
    }
    if (copy_count != request.byte_count) {
        *out_complete = false;
    }

    return MYLITE_OK;
}

static size_t group_concat_size_limit(uint64_t value) {
    if (value > (uint64_t)SIZE_MAX) {
        return SIZE_MAX;
    }
    return (size_t)value;
}

static int group_concat_reserve(struct mylite_group_concat_state *state, size_t needed) {
    char *resized = NULL;
    size_t capacity = 0U;

    if (state == NULL) {
        return MYLITE_MISUSE;
    }
    if (needed <= state->capacity) {
        return MYLITE_OK;
    }

    capacity = state->capacity == 0U ? (size_t)group_concat_initial_capacity : state->capacity;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2U) {
            capacity = needed;
            break;
        }
        capacity *= 2U;
    }
    resized = realloc(state->bytes, capacity);
    if (resized == NULL) {
        return MYLITE_NOMEM;
    }
    state->bytes = resized;
    state->capacity = capacity;
    return MYLITE_OK;
}

static size_t group_concat_valid_utf8_prefix(const unsigned char *bytes, size_t byte_count) {
    size_t offset = 0U;

    if (bytes == NULL) {
        return 0U;
    }
    while (offset < byte_count) {
        unsigned char first = bytes[offset];
        size_t sequence_length = 0U;

        if (first < utf8_ascii_upper_bound) {
            sequence_length = 1U;
        } else if (first >= utf8_two_byte_lead_min && first <= utf8_two_byte_lead_max) {
            sequence_length = 2U;
        } else if (first >= utf8_three_byte_lead_min && first <= utf8_three_byte_lead_max) {
            sequence_length = 3U;
        } else if (first >= utf8_four_byte_lead_min && first <= utf8_four_byte_lead_max) {
            sequence_length = 4U;
        } else {
            break;
        }
        if (sequence_length > byte_count - offset) {
            break;
        }
        for (size_t index = 1U; index < sequence_length; ++index) {
            if ((bytes[offset + index] & utf8_continuation_mask) != utf8_continuation_tag) {
                return offset;
            }
        }
        offset += sequence_length;
    }

    return offset;
}

static int group_concat_append_cut_warning(
    struct mylite_db *database,
    const struct mylite_group_concat_state *state
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = 0;

    if (database == NULL || state == NULL) {
        return MYLITE_MISUSE;
    }

    written = snprintf(
        message,
        sizeof(message),
        "Row %" PRIu64 " was cut by GROUP_CONCAT()",
        state->cut_ordinal
    );
    if (written < 0 || (size_t)written >= sizeof(message)) {
        return MYLITE_ERROR;
    }
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_group_concat_cut,
        "HY000",
        message
    );
}

static void group_concat_state_deinit(struct mylite_group_concat_state *state) {
    struct mylite_group_concat_distinct_value *current = NULL;

    if (state == NULL) {
        return;
    }
    current = state->distinct_values;
    while (current != NULL) {
        struct mylite_group_concat_distinct_value *next = current->next;

        free(current->bytes);
        free(current);
        current = next;
    }
    free(state->bytes);
    *state = (struct mylite_group_concat_state){0};
}
