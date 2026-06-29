#include "mylite_json_aggregate.h"

#include <mylite/mylite.h>

#include "mylite_json.h"
#include "mylite_sqlite_registration.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    json_aggregate_array_argc = 2,
    json_aggregate_object_argc = 4,
    json_aggregate_initial_capacity = 8,
};

struct json_aggregate_value {
    struct mylite_json_sql_value value;
    char *owned_text;
};

struct json_aggregate_object_pair {
    struct json_aggregate_value key;
    struct json_aggregate_value value;
};

struct json_aggregate_state {
    struct json_aggregate_value *keys;
    struct json_aggregate_value *values;
    size_t count;
    size_t capacity;
};

static void json_arrayagg_step(sqlite3_context *context, int argc, sqlite3_value **argv);
static void json_objectagg_step(sqlite3_context *context, int argc, sqlite3_value **argv);
static void json_arrayagg_final(sqlite3_context *context);
static void json_objectagg_final(sqlite3_context *context);
static int json_aggregate_convert_value(
    sqlite3_value *tag_value,
    sqlite3_value *sqlite_value,
    bool allow_json,
    struct json_aggregate_value *out_value
);
static bool json_aggregate_value_kind_is_valid(int64_t kind, bool allow_json);
static int json_aggregate_copy_sqlite_text(
    sqlite3_value *sqlite_value,
    struct json_aggregate_value *out_value
);
static int json_aggregate_append_array_value(
    struct json_aggregate_state *state,
    struct json_aggregate_value *value
);
static int json_aggregate_append_object_pair(
    struct json_aggregate_state *state,
    struct json_aggregate_object_pair *pair
);
static int json_aggregate_reserve(struct json_aggregate_state *state, bool has_keys);
static void json_aggregate_finish_array(
    sqlite3_context *context,
    struct json_aggregate_state *state
);
static void json_aggregate_finish_object(
    sqlite3_context *context,
    struct json_aggregate_state *state
);
static void json_aggregate_finish_result(
    sqlite3_context *context,
    int rc,
    char *result,
    size_t result_length,
    const struct mylite_json_normalize_result *normalize_result
);
static void json_aggregate_value_deinit(struct json_aggregate_value *value);
static void json_aggregate_object_pair_deinit(struct json_aggregate_object_pair *pair);
static void json_aggregate_state_deinit(struct json_aggregate_state *state);

int mylite_sqlite_register_json_aggregate_functions(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_AGGREGATE,
            .name = "_mylite_json_arrayagg",
            .argument_count = json_aggregate_array_argc,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = NULL,
            .step_callback = json_arrayagg_step,
            .final_callback = json_arrayagg_final,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_AGGREGATE,
            .name = "_mylite_json_objectagg",
            .argument_count = json_aggregate_object_argc,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
            .application_data = NULL,
            .scalar_callback = NULL,
            .step_callback = json_objectagg_step,
            .final_callback = json_objectagg_final,
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

static void json_arrayagg_step(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct json_aggregate_state *state = NULL;
    struct json_aggregate_value value = {0};
    int rc = MYLITE_OK;

    if (context == NULL) {
        return;
    }
    if (argc != json_aggregate_array_argc || argv == NULL || argv[0] == NULL || argv[1] == NULL) {
        sqlite3_result_error(context, "invalid MyLite JSON_ARRAYAGG callback", -1);
        return;
    }

    rc = json_aggregate_convert_value(argv[0], argv[1], true, &value);
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK) {
        sqlite3_result_error(context, "Unsupported JSON value in JSON constructor", -1);
        return;
    }

    state = sqlite3_aggregate_context(context, (int)sizeof(*state));
    if (state == NULL) {
        json_aggregate_value_deinit(&value);
        sqlite3_result_error_nomem(context);
        return;
    }

    rc = json_aggregate_append_array_value(state, &value);
    if (rc == MYLITE_NOMEM) {
        json_aggregate_value_deinit(&value);
        sqlite3_result_error_nomem(context);
    } else if (rc != MYLITE_OK) {
        json_aggregate_value_deinit(&value);
        sqlite3_result_error(context, "MyLite JSON_ARRAYAGG append failed", -1);
    }
}

static void json_objectagg_step(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct json_aggregate_state *state = NULL;
    struct json_aggregate_object_pair pair = {0};
    int rc = MYLITE_OK;

    if (context == NULL) {
        return;
    }
    if (argc != json_aggregate_object_argc || argv == NULL || argv[0] == NULL || argv[1] == NULL ||
        argv[2] == NULL || argv[3] == NULL) {
        sqlite3_result_error(context, "invalid MyLite JSON_OBJECTAGG callback", -1);
        return;
    }

    rc = json_aggregate_convert_value(argv[0], argv[1], false, &pair.key);
    if (rc == MYLITE_OK) {
        rc = json_aggregate_convert_value(argv[2], argv[3], true, &pair.value);
    }
    if (rc == MYLITE_NOMEM) {
        json_aggregate_object_pair_deinit(&pair);
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK) {
        json_aggregate_object_pair_deinit(&pair);
        sqlite3_result_error(context, "Unsupported JSON value in JSON constructor", -1);
        return;
    }
    if (pair.key.value.kind == MYLITE_JSON_SQL_VALUE_NULL) {
        json_aggregate_object_pair_deinit(&pair);
        sqlite3_result_error(context, "JSON documents may not contain NULL member names.", -1);
        return;
    }

    state = sqlite3_aggregate_context(context, (int)sizeof(*state));
    if (state == NULL) {
        json_aggregate_object_pair_deinit(&pair);
        sqlite3_result_error_nomem(context);
        return;
    }

    rc = json_aggregate_append_object_pair(state, &pair);
    if (rc == MYLITE_NOMEM) {
        json_aggregate_object_pair_deinit(&pair);
        sqlite3_result_error_nomem(context);
    } else if (rc != MYLITE_OK) {
        json_aggregate_object_pair_deinit(&pair);
        sqlite3_result_error(context, "MyLite JSON_OBJECTAGG append failed", -1);
    }
}

static void json_arrayagg_final(sqlite3_context *context) {
    struct json_aggregate_state *state = NULL;

    if (context == NULL) {
        return;
    }
    state = sqlite3_aggregate_context(context, 0);
    if (state == NULL || state->count == 0U) {
        json_aggregate_state_deinit(state);
        sqlite3_result_null(context);
        return;
    }

    json_aggregate_finish_array(context, state);
}

static void json_objectagg_final(sqlite3_context *context) {
    struct json_aggregate_state *state = NULL;

    if (context == NULL) {
        return;
    }
    state = sqlite3_aggregate_context(context, 0);
    if (state == NULL || state->count == 0U) {
        json_aggregate_state_deinit(state);
        sqlite3_result_null(context);
        return;
    }

    json_aggregate_finish_object(context, state);
}

static int json_aggregate_convert_value(
    sqlite3_value *tag_value,
    sqlite3_value *sqlite_value,
    bool allow_json,
    struct json_aggregate_value *out_value
) {
    int64_t kind = 0;

    if (tag_value == NULL || sqlite_value == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct json_aggregate_value){0};

    if (sqlite3_value_type(tag_value) != SQLITE_INTEGER) {
        return MYLITE_ERROR;
    }
    kind = sqlite3_value_int64(tag_value);
    if (!json_aggregate_value_kind_is_valid(kind, allow_json)) {
        return MYLITE_ERROR;
    }

    out_value->value.kind = (enum mylite_json_sql_value_kind)kind;
    if (sqlite3_value_type(sqlite_value) == SQLITE_NULL) {
        out_value->value.kind = MYLITE_JSON_SQL_VALUE_NULL;
        return MYLITE_OK;
    }

    switch (out_value->value.kind) {
    case MYLITE_JSON_SQL_VALUE_NULL:
        return MYLITE_OK;
    case MYLITE_JSON_SQL_VALUE_INTEGER:
        if (sqlite3_value_type(sqlite_value) != SQLITE_INTEGER) {
            return MYLITE_ERROR;
        }
        out_value->value.integer = (int64_t)sqlite3_value_int64(sqlite_value);
        return MYLITE_OK;
    case MYLITE_JSON_SQL_VALUE_BOOLEAN:
        if (sqlite3_value_type(sqlite_value) != SQLITE_INTEGER) {
            return MYLITE_ERROR;
        }
        out_value->value.boolean = sqlite3_value_int64(sqlite_value) != 0;
        return MYLITE_OK;
    case MYLITE_JSON_SQL_VALUE_STRING:
    case MYLITE_JSON_SQL_VALUE_JSON:
        return json_aggregate_copy_sqlite_text(sqlite_value, out_value);
    }

    return MYLITE_ERROR;
}

static bool json_aggregate_value_kind_is_valid(int64_t kind, bool allow_json) {
    if (kind == MYLITE_JSON_SQL_VALUE_NULL || kind == MYLITE_JSON_SQL_VALUE_INTEGER ||
        kind == MYLITE_JSON_SQL_VALUE_BOOLEAN || kind == MYLITE_JSON_SQL_VALUE_STRING) {
        return true;
    }
    return allow_json && kind == MYLITE_JSON_SQL_VALUE_JSON;
}

static int json_aggregate_copy_sqlite_text(
    sqlite3_value *sqlite_value,
    struct json_aggregate_value *out_value
) {
    const unsigned char *text = sqlite3_value_text(sqlite_value);
    int byte_count = sqlite3_value_bytes(sqlite_value);
    size_t text_length = 0U;

    if (byte_count < 0) {
        return MYLITE_NOMEM;
    }
    text_length = (size_t)byte_count;
    if (text == NULL && text_length != 0U) {
        return MYLITE_NOMEM;
    }

    out_value->owned_text = malloc(text_length + 1U);
    if (out_value->owned_text == NULL) {
        return MYLITE_NOMEM;
    }
    if (text_length != 0U) {
        memcpy(out_value->owned_text, text, text_length);
    }
    out_value->owned_text[text_length] = '\0';
    out_value->value.text = out_value->owned_text;
    out_value->value.text_length = text_length;
    return MYLITE_OK;
}

static int json_aggregate_append_array_value(
    struct json_aggregate_state *state,
    struct json_aggregate_value *value
) {
    int rc = json_aggregate_reserve(state, false);

    if (rc != MYLITE_OK) {
        return rc;
    }

    state->values[state->count] = *value;
    *value = (struct json_aggregate_value){0};
    ++state->count;
    return MYLITE_OK;
}

static int json_aggregate_append_object_pair(
    struct json_aggregate_state *state,
    struct json_aggregate_object_pair *pair
) {
    int rc = json_aggregate_reserve(state, true);

    if (rc != MYLITE_OK) {
        return rc;
    }

    state->keys[state->count] = pair->key;
    state->values[state->count] = pair->value;
    *pair = (struct json_aggregate_object_pair){0};
    ++state->count;
    return MYLITE_OK;
}

static int json_aggregate_reserve(struct json_aggregate_state *state, bool has_keys) {
    struct json_aggregate_value *values = NULL;
    struct json_aggregate_value *keys = NULL;
    size_t next_capacity = 0U;

    if (state == NULL) {
        return MYLITE_MISUSE;
    }
    if (state->count < state->capacity) {
        return MYLITE_OK;
    }
    if (state->capacity == 0U) {
        next_capacity = json_aggregate_initial_capacity;
    } else {
        if (state->capacity > SIZE_MAX / 2U) {
            return MYLITE_NOMEM;
        }
        next_capacity = state->capacity * 2U;
    }
    if (next_capacity > SIZE_MAX / sizeof(*state->values)) {
        return MYLITE_NOMEM;
    }

    values = realloc(state->values, next_capacity * sizeof(*state->values));
    if (values == NULL) {
        return MYLITE_NOMEM;
    }
    state->values = values;
    if (has_keys) {
        keys = realloc(state->keys, next_capacity * sizeof(*state->keys));
        if (keys == NULL) {
            return MYLITE_NOMEM;
        }
        state->keys = keys;
    }
    state->capacity = next_capacity;
    return MYLITE_OK;
}

static void json_aggregate_finish_array(
    sqlite3_context *context,
    struct json_aggregate_state *state
) {
    struct mylite_json_sql_value *values = NULL;
    struct mylite_json_normalize_result normalize_result = {0};
    char *result = NULL;
    size_t result_length = 0U;
    int rc = MYLITE_OK;

    if (state->count > SIZE_MAX / sizeof(*values)) {
        sqlite3_result_error_nomem(context);
        json_aggregate_state_deinit(state);
        return;
    }
    values = calloc(state->count, sizeof(*values));
    if (values == NULL) {
        sqlite3_result_error_nomem(context);
        json_aggregate_state_deinit(state);
        return;
    }
    for (size_t index = 0U; index < state->count; ++index) {
        values[index] = state->values[index].value;
    }

    rc = mylite_json_array_from_sql_values(
        values,
        state->count,
        &result,
        &result_length,
        &normalize_result
    );
    json_aggregate_finish_result(context, rc, result, result_length, &normalize_result);
    free(values);
    json_aggregate_state_deinit(state);
}

static void json_aggregate_finish_object(
    sqlite3_context *context,
    struct json_aggregate_state *state
) {
    struct mylite_json_sql_value *keys = NULL;
    struct mylite_json_sql_value *values = NULL;
    struct mylite_json_normalize_result normalize_result = {0};
    char *result = NULL;
    size_t result_length = 0U;
    int rc = MYLITE_OK;

    if (state->count > SIZE_MAX / sizeof(*values)) {
        sqlite3_result_error_nomem(context);
        json_aggregate_state_deinit(state);
        return;
    }
    keys = calloc(state->count, sizeof(*keys));
    values = calloc(state->count, sizeof(*values));
    if (keys == NULL || values == NULL) {
        free(keys);
        free(values);
        sqlite3_result_error_nomem(context);
        json_aggregate_state_deinit(state);
        return;
    }
    for (size_t index = 0U; index < state->count; ++index) {
        keys[index] = state->keys[index].value;
        values[index] = state->values[index].value;
    }

    rc = mylite_json_object_from_sql_values(
        keys,
        values,
        state->count,
        &result,
        &result_length,
        &normalize_result
    );
    json_aggregate_finish_result(context, rc, result, result_length, &normalize_result);
    free(keys);
    free(values);
    json_aggregate_state_deinit(state);
}

static void json_aggregate_finish_result(
    sqlite3_context *context,
    int rc,
    char *result,
    size_t result_length,
    const struct mylite_json_normalize_result *normalize_result
) {
    if (rc == MYLITE_OK) {
        sqlite3_result_text64(
            context,
            result == NULL ? "" : result,
            (sqlite3_uint64)result_length,
            SQLITE_TRANSIENT,
            SQLITE_UTF8
        );
        free(result);
        return;
    }
    free(result);
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (normalize_result != NULL && normalize_result->status == MYLITE_JSON_NORMALIZE_INVALID) {
        sqlite3_result_error(context, "Invalid JSON text in JSON constructor", -1);
        return;
    }
    if (normalize_result != NULL && normalize_result->status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
        sqlite3_result_error(context, "Unsupported JSON value in JSON constructor", -1);
        return;
    }
    sqlite3_result_error(context, "MyLite JSON aggregate failed", -1);
}

static void json_aggregate_value_deinit(struct json_aggregate_value *value) {
    if (value == NULL) {
        return;
    }
    free(value->owned_text);
    *value = (struct json_aggregate_value){0};
}

static void json_aggregate_object_pair_deinit(struct json_aggregate_object_pair *pair) {
    if (pair == NULL) {
        return;
    }
    json_aggregate_value_deinit(&pair->key);
    json_aggregate_value_deinit(&pair->value);
}

static void json_aggregate_state_deinit(struct json_aggregate_state *state) {
    if (state == NULL) {
        return;
    }

    for (size_t index = 0U; index < state->count; ++index) {
        if (state->keys != NULL) {
            json_aggregate_value_deinit(&state->keys[index]);
        }
        json_aggregate_value_deinit(&state->values[index]);
    }
    free(state->keys);
    free(state->values);
    *state = (struct json_aggregate_state){0};
}
