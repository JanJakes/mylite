#include "mylite_sqlite_bootstrap.h"

#include "mylite_base_conversion.h"
#include "mylite_bitwise_aggregate.h"
#include "mylite_cast_convert.h"
#include "mylite_convert_tz.h"
#include "mylite_date_format.h"
#include "mylite_date_interval_second.h"
#include "mylite_datediff.h"
#include "mylite_digest.h"
#include "mylite_group_concat_aggregate.h"
#include "mylite_integer_arithmetic.h"
#include "mylite_json_functions.h"
#include "mylite_like.h"
#include "mylite_numeric_functions.h"
#include "mylite_period_functions.h"
#include "mylite_rand.h"
#include "mylite_random_bytes.h"
#include "mylite_regexp.h"
#include "mylite_sqlite_registration.h"
#include "mylite_string_base64.h"
#include "mylite_string_bitmask.h"
#include "mylite_string_case.h"
#include "mylite_string_char.h"
#include "mylite_string_codepoint.h"
#include "mylite_string_compression.h"
#include "mylite_string_concat.h"
#include "mylite_string_insert.h"
#include "mylite_string_padding.h"
#include "mylite_string_quote.h"
#include "mylite_string_replace.h"
#include "mylite_string_reverse.h"
#include "mylite_string_search.h"
#include "mylite_string_soundex.h"
#include "mylite_string_substring_index.h"
#include "mylite_string_trim.h"
#include "mylite_string_unhex.h"
#include "mylite_temporal_constructor.h"
#include "mylite_temporal_extract.h"
#include "mylite_timediff.h"
#include "mylite_timestamp_function.h"
#include "mylite_timestampdiff.h"
#include "mylite_unix_timestamp.h"
#include "mylite_uuid.h"
#include "mylite_weight_string.h"
#include "sqlite3.h"

#include <stddef.h>
#include <string.h>

static const char *owner_client_data_key(void);
static int attach_owner_client_data(
    sqlite3 *sqlite,
    struct mylite_db *owner,
    struct mylite_sqlite_bootstrap_state *state
);
static int apply_connection_policy(sqlite3 *sqlite, struct mylite_sqlite_bootstrap_state *state);
static int apply_trusted_schema_policy(
    sqlite3 *sqlite,
    struct mylite_sqlite_bootstrap_state *state
);
static int apply_foreign_key_policy(sqlite3 *sqlite, struct mylite_sqlite_bootstrap_state *state);
static int initialize_function_registration_surface(
    sqlite3 *sqlite,
    struct mylite_sqlite_bootstrap_state *state
);
static int initialize_string_function_registration_surface(sqlite3 *sqlite);
static int initialize_temporal_core_function_registration_surface(sqlite3 *sqlite);
static int initialize_collation_registration_surface(
    sqlite3 *sqlite,
    struct mylite_sqlite_bootstrap_state *state
);
static int compare_utf8mb4_0900_ai_ci_ascii(
    void *application_data,
    int left_size,
    const void *left,
    int right_size,
    const void *right
);
static int compare_utf8mb4_0900_as_cs_ascii(
    void *application_data,
    int left_size,
    const void *left,
    int right_size,
    const void *right
);
static unsigned char ascii_collation_fold(unsigned char byte);
static unsigned char ascii_collation_case_weight(unsigned char byte);
static void initialize_hook_registration_surface(struct mylite_sqlite_bootstrap_state *state);
static void clear_hook_registration_surface(
    sqlite3 *sqlite,
    struct mylite_sqlite_hook_registration_state *hooks
);

int mylite_sqlite_bootstrap_connection(
    sqlite3 *sqlite,
    struct mylite_db *owner,
    struct mylite_sqlite_bootstrap_state *state
) {
    int rc = MYLITE_OK;

    if (sqlite == NULL || owner == NULL || state == NULL) {
        return MYLITE_MISUSE;
    }

    memset(state, 0, sizeof(*state));

    rc = attach_owner_client_data(sqlite, owner, state);
    if (rc != MYLITE_OK) {
        goto cleanup;
    }
    rc = apply_connection_policy(sqlite, state);
    if (rc != MYLITE_OK) {
        goto cleanup;
    }
    rc = initialize_function_registration_surface(sqlite, state);
    if (rc != MYLITE_OK) {
        goto cleanup;
    }
    rc = initialize_collation_registration_surface(sqlite, state);
    if (rc != MYLITE_OK) {
        goto cleanup;
    }
    initialize_hook_registration_surface(state);

    state->initialized = true;

    return MYLITE_OK;

cleanup:
    mylite_sqlite_bootstrap_deinit(sqlite, state);
    return rc;
}

void mylite_sqlite_bootstrap_deinit(sqlite3 *sqlite, struct mylite_sqlite_bootstrap_state *state) {
    if (state == NULL) {
        return;
    }

    if (sqlite != NULL) {
        clear_hook_registration_surface(sqlite, &state->hooks);
        if (state->owner_client_data_is_registered) {
            (void)sqlite3_set_clientdata(sqlite, owner_client_data_key(), NULL, NULL);
        }
    }

    memset(state, 0, sizeof(*state));
}

struct mylite_db *mylite_sqlite_bootstrap_owner_from_connection(sqlite3 *sqlite) {
    if (sqlite == NULL) {
        return NULL;
    }

    return sqlite3_get_clientdata(sqlite, owner_client_data_key());
}

struct mylite_db *mylite_sqlite_bootstrap_owner_from_context(sqlite3_context *context) {
    if (context == NULL) {
        return NULL;
    }

    return mylite_sqlite_bootstrap_owner_from_connection(sqlite3_context_db_handle(context));
}

static const char *owner_client_data_key(void) {
    return "mylite.connection.owner";
}

static int attach_owner_client_data(
    sqlite3 *sqlite,
    struct mylite_db *owner,
    struct mylite_sqlite_bootstrap_state *state
) {
    int rc = sqlite3_set_clientdata(sqlite, owner_client_data_key(), owner, NULL);

    if (rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(rc);
    }

    state->owner_client_data_is_registered = true;

    return MYLITE_OK;
}

static int apply_connection_policy(sqlite3 *sqlite, struct mylite_sqlite_bootstrap_state *state) {
    int rc = apply_trusted_schema_policy(sqlite, state);

    if (rc != MYLITE_OK) {
        return rc;
    }

    return apply_foreign_key_policy(sqlite, state);
}

static int apply_trusted_schema_policy(
    sqlite3 *sqlite,
    struct mylite_sqlite_bootstrap_state *state
) {
    int trusted_schema_is_enabled = 1;
    int rc =
        sqlite3_db_config(sqlite, SQLITE_DBCONFIG_TRUSTED_SCHEMA, 0, &trusted_schema_is_enabled);

    if (rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(rc);
    }

    state->trusted_schema_policy_is_applied = true;
    state->trusted_schema_is_enabled = trusted_schema_is_enabled != 0;

    return MYLITE_OK;
}

static int apply_foreign_key_policy(sqlite3 *sqlite, struct mylite_sqlite_bootstrap_state *state) {
    int foreign_key_enforcement_is_enabled = 1;
    int rc = sqlite3_db_config(
        sqlite,
        SQLITE_DBCONFIG_ENABLE_FKEY,
        0,
        &foreign_key_enforcement_is_enabled
    );

    if (rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(rc);
    }

    state->foreign_key_policy_is_applied = true;
    state->foreign_key_enforcement_is_enabled = foreign_key_enforcement_is_enabled != 0;
    state->foreign_key_policy_is_placeholder = true;

    return MYLITE_OK;
}

static int initialize_function_registration_surface(
    sqlite3 *sqlite,
    struct mylite_sqlite_bootstrap_state *state
) {
    int rc = mylite_sqlite_register_functions(sqlite, NULL, 0U);

    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_bitwise_aggregate_functions(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_group_concat_aggregate_function(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = initialize_temporal_core_function_registration_surface(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_integer_arithmetic_functions(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_numeric_functions(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_base_conversion_functions(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_cast_convert_functions(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_temporal_extract_function(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_temporal_constructor_functions(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_period_functions(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_convert_tz_function(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_timestampdiff_function(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_timediff_function(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_unix_timestamp_function(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = initialize_string_function_registration_surface(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_digest_functions(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_uuid_functions(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_regexp_functions(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_like_functions(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_json_functions(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_rand_functions(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_random_bytes_function(sqlite);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    state->function_registration_surface_is_initialized = true;

    return MYLITE_OK;
}

static int initialize_string_function_registration_surface(sqlite3 *sqlite) {
    int rc = mylite_sqlite_register_string_case_functions(sqlite);

    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_string_char_function(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_string_codepoint_functions(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_string_concat_functions(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_string_insert_function(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_string_padding_functions(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_string_bitmask_functions(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_string_quote_function(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_string_replace_function(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_string_reverse_function(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_string_search_functions(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_string_soundex_function(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_string_substring_index_function(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_string_trim_functions(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_string_base64_functions(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_string_compression_functions(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_string_unhex_function(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_weight_string_functions(sqlite);
    }
    return rc;
}

static int initialize_temporal_core_function_registration_surface(sqlite3 *sqlite) {
    int rc = mylite_sqlite_register_date_format_function(sqlite);

    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_date_interval_second_function(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_datediff_function(sqlite);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_sqlite_register_timestamp_function(sqlite);
    }
    return rc;
}

static int initialize_collation_registration_surface(
    sqlite3 *sqlite,
    struct mylite_sqlite_bootstrap_state *state
) {
    static const struct mylite_sqlite_collation_registration registrations[] = {
        {
            .name = "utf8mb4_0900_ai_ci",
            .text_representation = SQLITE_UTF8,
            .application_data = NULL,
            .compare_callback = compare_utf8mb4_0900_ai_ci_ascii,
            .destroy_callback = NULL,
        },
        {
            .name = "utf8mb4_0900_as_cs",
            .text_representation = SQLITE_UTF8,
            .application_data = NULL,
            .compare_callback = compare_utf8mb4_0900_as_cs_ascii,
            .destroy_callback = NULL,
        },
    };
    int rc = mylite_sqlite_register_collations(
        sqlite,
        registrations,
        sizeof(registrations) / sizeof(registrations[0])
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    state->collation_registration_surface_is_initialized = true;

    return MYLITE_OK;
}

static int compare_utf8mb4_0900_ai_ci_ascii(
    void *application_data,
    int left_size,
    const void *left,
    int right_size,
    const void *right
) {
    const unsigned char *left_text = left;
    const unsigned char *right_text = right;
    int shared_size = left_size < right_size ? left_size : right_size;

    (void)application_data;
    for (int index = 0; index < shared_size; ++index) {
        unsigned char left_byte = ascii_collation_fold(left_text[index]);
        unsigned char right_byte = ascii_collation_fold(right_text[index]);

        if (left_byte < right_byte) {
            return -1;
        }
        if (left_byte > right_byte) {
            return 1;
        }
    }
    if (left_size < right_size) {
        return -1;
    }
    if (left_size > right_size) {
        return 1;
    }
    return 0;
}

static int compare_utf8mb4_0900_as_cs_ascii(
    void *application_data,
    int left_size,
    const void *left,
    int right_size,
    const void *right
) {
    const unsigned char *left_text = left;
    const unsigned char *right_text = right;
    int shared_size = left_size < right_size ? left_size : right_size;

    (void)application_data;
    for (int index = 0; index < shared_size; ++index) {
        unsigned char left_byte = ascii_collation_fold(left_text[index]);
        unsigned char right_byte = ascii_collation_fold(right_text[index]);

        if (left_byte < right_byte) {
            return -1;
        }
        if (left_byte > right_byte) {
            return 1;
        }
    }
    if (left_size < right_size) {
        return -1;
    }
    if (left_size > right_size) {
        return 1;
    }

    for (int index = 0; index < shared_size; ++index) {
        unsigned char left_weight = ascii_collation_case_weight(left_text[index]);
        unsigned char right_weight = ascii_collation_case_weight(right_text[index]);

        if (left_weight < right_weight) {
            return -1;
        }
        if (left_weight > right_weight) {
            return 1;
        }
    }
    return 0;
}

static unsigned char ascii_collation_fold(unsigned char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (unsigned char)(byte + ('a' - 'A'));
    }

    return byte;
}

static unsigned char ascii_collation_case_weight(unsigned char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return 1U;
    }
    return 0U;
}

static void initialize_hook_registration_surface(struct mylite_sqlite_bootstrap_state *state) {
    state->hooks.registration_surface_is_initialized = true;
}

static void clear_hook_registration_surface(
    sqlite3 *sqlite,
    struct mylite_sqlite_hook_registration_state *hooks
) {
    if (sqlite == NULL || hooks == NULL || !hooks->registration_surface_is_initialized) {
        return;
    }

    if (hooks->busy_handler_is_registered) {
        (void)sqlite3_busy_handler(sqlite, NULL, NULL);
    }
    if (hooks->progress_handler_is_registered) {
        sqlite3_progress_handler(sqlite, 0, NULL, NULL);
    }
    if (hooks->trace_callback_is_registered) {
        (void)sqlite3_trace_v2(sqlite, 0U, NULL, NULL);
    }
    if (hooks->update_hook_is_registered) {
        (void)sqlite3_update_hook(sqlite, NULL, NULL);
    }
    if (hooks->commit_hook_is_registered) {
        (void)sqlite3_commit_hook(sqlite, NULL, NULL);
    }
    if (hooks->rollback_hook_is_registered) {
        (void)sqlite3_rollback_hook(sqlite, NULL, NULL);
    }

    memset(hooks, 0, sizeof(*hooks));
}
