#include "mylite_json_functions.h"

#include <mylite/mylite.h>

#include "mylite_json.h"
#include "mylite_sqlite_registration.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static void json_array_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void json_object_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void json_extract_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void json_unquote_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void json_valid_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static int collect_json_array_sql_values(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv,
    struct mylite_json_sql_value **out_values
);
static int collect_json_object_sql_values(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv,
    struct mylite_json_sql_value **out_keys,
    struct mylite_json_sql_value **out_values,
    size_t *out_pair_count
);
static int json_sql_value_from_sqlite(
    sqlite3_context *context,
    sqlite3_value *tag_value,
    sqlite3_value *value,
    bool allow_json,
    struct mylite_json_sql_value *out_value
);
static bool json_sql_value_kind_is_valid(int64_t kind);
static void finish_json_constructor_sqlite_result(
    sqlite3_context *context,
    int rc,
    char *result,
    size_t result_length,
    const struct mylite_json_normalize_result *normalize_result
);

int mylite_sqlite_register_json_functions(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_json_array",
            .argument_count = -1,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = NULL,
            .scalar_callback = json_array_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_json_object",
            .argument_count = -1,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = NULL,
            .scalar_callback = json_object_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_json_valid",
            .argument_count = 1,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = NULL,
            .scalar_callback = json_valid_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_json_extract",
            .argument_count = 2,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = NULL,
            .scalar_callback = json_extract_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_json_unquote",
            .argument_count = 1,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = NULL,
            .scalar_callback = json_unquote_sqlite_callback,
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

static void json_array_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_json_sql_value *values = NULL;
    char *result = NULL;
    size_t result_length = 0U;
    size_t value_count = 0U;
    struct mylite_json_normalize_result normalize_result = {0};
    int rc = MYLITE_OK;

    rc = collect_json_array_sql_values(context, argc, argv, &values);
    if (rc != MYLITE_OK) {
        return;
    }
    value_count = (size_t)argc / 2U;
    rc = mylite_json_array_from_sql_values(
        values,
        value_count,
        &result,
        &result_length,
        &normalize_result
    );
    free(values);
    finish_json_constructor_sqlite_result(context, rc, result, result_length, &normalize_result);
}

static void json_object_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    struct mylite_json_sql_value *keys = NULL;
    struct mylite_json_sql_value *values = NULL;
    char *result = NULL;
    size_t pair_count = 0U;
    size_t result_length = 0U;
    struct mylite_json_normalize_result normalize_result = {0};
    int rc = MYLITE_OK;

    rc = collect_json_object_sql_values(context, argc, argv, &keys, &values, &pair_count);
    if (rc != MYLITE_OK) {
        return;
    }
    rc = mylite_json_object_from_sql_values(
        keys,
        values,
        pair_count,
        &result,
        &result_length,
        &normalize_result
    );
    free(values);
    free(keys);
    finish_json_constructor_sqlite_result(context, rc, result, result_length, &normalize_result);
}

static void json_extract_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const unsigned char *document = NULL;
    const unsigned char *path = NULL;
    int document_length = 0;
    int path_length = 0;
    char *result = NULL;
    size_t result_length = 0U;
    bool is_null = false;
    struct mylite_json_normalize_result normalize_result = {0};
    int rc = MYLITE_OK;

    if (context == NULL || argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL) {
        sqlite3_result_error(context, "invalid MyLite JSON_EXTRACT callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT || sqlite3_value_type(argv[1]) != SQLITE_TEXT) {
        sqlite3_result_error(context, "Invalid data type for JSON data in JSON_EXTRACT()", -1);
        return;
    }

    document = sqlite3_value_text(argv[0]);
    path = sqlite3_value_text(argv[1]);
    document_length = sqlite3_value_bytes(argv[0]);
    path_length = sqlite3_value_bytes(argv[1]);
    if (document == NULL || path == NULL || document_length < 0 || path_length < 0) {
        sqlite3_result_error_nomem(context);
        return;
    }

    rc = mylite_json_extract(
        (const char *)document,
        (size_t)document_length,
        (const char *)path,
        (size_t)path_length,
        &result,
        &result_length,
        &is_null,
        &normalize_result
    );
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK) {
        if (normalize_result.status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
            sqlite3_result_error(
                context,
                "Unsupported JSON path or JSON document in JSON_EXTRACT()",
                -1
            );
        } else if (normalize_result.status == MYLITE_JSON_NORMALIZE_INVALID) {
            sqlite3_result_error(context, "Invalid JSON text or JSON path in JSON_EXTRACT()", -1);
        } else {
            sqlite3_result_error(context, "MyLite JSON_EXTRACT failed", -1);
        }
        return;
    }
    if (is_null) {
        sqlite3_result_null(context);
        return;
    }
    if (result_length > (size_t)INT_MAX) {
        free(result);
        sqlite3_result_error_nomem(context);
        return;
    }
    sqlite3_result_text(context, result, (int)result_length, free);
}

static void json_unquote_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const unsigned char *text = NULL;
    int text_length = 0;
    char *result = NULL;
    size_t result_length = 0U;
    struct mylite_json_normalize_result normalize_result = {0};
    int rc = MYLITE_OK;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite JSON_UNQUOTE callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT) {
        sqlite3_result_error(context, "Incorrect type for argument to JSON_UNQUOTE()", -1);
        return;
    }

    text = sqlite3_value_text(argv[0]);
    text_length = sqlite3_value_bytes(argv[0]);
    if (text == NULL || text_length < 0) {
        sqlite3_result_error_nomem(context);
        return;
    }

    rc = mylite_json_unquote(
        (const char *)text,
        (size_t)text_length,
        &result,
        &result_length,
        &normalize_result
    );
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK) {
        if (normalize_result.status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
            sqlite3_result_error(context, "Unsupported JSON string in JSON_UNQUOTE()", -1);
        } else {
            sqlite3_result_error(context, "Invalid JSON text in JSON_UNQUOTE()", -1);
        }
        return;
    }
    if (result_length > (size_t)INT_MAX) {
        free(result);
        sqlite3_result_error_nomem(context);
        return;
    }
    sqlite3_result_text(context, result, (int)result_length, free);
}

static void json_valid_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const unsigned char *text = NULL;
    int text_length = 0;
    bool is_valid = false;
    int rc = MYLITE_OK;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite JSON_VALID callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT) {
        sqlite3_result_int64(context, 0);
        return;
    }

    text = sqlite3_value_text(argv[0]);
    text_length = sqlite3_value_bytes(argv[0]);
    if (text == NULL || text_length < 0) {
        sqlite3_result_error_nomem(context);
        return;
    }

    rc = mylite_json_validate((const char *)text, (size_t)text_length, &is_valid);
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK) {
        sqlite3_result_error(context, "MyLite JSON_VALID failed", -1);
        return;
    }

    if (is_valid) {
        sqlite3_result_int64(context, 1);
    } else {
        sqlite3_result_int64(context, 0);
    }
}

static int collect_json_array_sql_values(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv,
    struct mylite_json_sql_value **out_values
) {
    struct mylite_json_sql_value *values = NULL;
    size_t value_count = 0U;

    if (out_values == NULL) {
        sqlite3_result_error(context, "invalid MyLite JSON_ARRAY callback", -1);
        return MYLITE_ERROR;
    }
    *out_values = NULL;
    if (argc < 0 || (argc % 2) != 0 || (argc != 0 && argv == NULL)) {
        sqlite3_result_error(context, "invalid MyLite JSON_ARRAY callback", -1);
        return MYLITE_ERROR;
    }
    value_count = (size_t)argc / 2U;
    if (value_count > SIZE_MAX / sizeof(*values)) {
        sqlite3_result_error_nomem(context);
        return MYLITE_NOMEM;
    }
    values = value_count == 0U
                 ? NULL
                 : (struct mylite_json_sql_value *)calloc(value_count, sizeof(*values));
    if (value_count != 0U && values == NULL) {
        sqlite3_result_error_nomem(context);
        return MYLITE_NOMEM;
    }

    for (size_t value_index = 0U; value_index < value_count; ++value_index) {
        int rc = json_sql_value_from_sqlite(
            context,
            argv[value_index * 2U],
            argv[(value_index * 2U) + 1U],
            true,
            &values[value_index]
        );

        if (rc != MYLITE_OK) {
            free(values);
            return rc;
        }
    }

    *out_values = values;
    return MYLITE_OK;
}

static int collect_json_object_sql_values(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv,
    struct mylite_json_sql_value **out_keys,
    struct mylite_json_sql_value **out_values,
    size_t *out_pair_count
) {
    struct mylite_json_sql_value *keys = NULL;
    struct mylite_json_sql_value *values = NULL;
    size_t pair_count = 0U;

    if (out_keys == NULL || out_values == NULL || out_pair_count == NULL) {
        sqlite3_result_error(context, "invalid MyLite JSON_OBJECT callback", -1);
        return MYLITE_ERROR;
    }
    *out_keys = NULL;
    *out_values = NULL;
    *out_pair_count = 0U;
    if (argc < 0 || (argc % 4) != 0 || (argc != 0 && argv == NULL)) {
        sqlite3_result_error(context, "invalid MyLite JSON_OBJECT callback", -1);
        return MYLITE_ERROR;
    }
    pair_count = (size_t)argc / 4U;
    if (pair_count > SIZE_MAX / sizeof(*keys) || pair_count > SIZE_MAX / sizeof(*values)) {
        sqlite3_result_error_nomem(context);
        return MYLITE_NOMEM;
    }
    keys =
        pair_count == 0U ? NULL : (struct mylite_json_sql_value *)calloc(pair_count, sizeof(*keys));
    values = pair_count == 0U ? NULL
                              : (struct mylite_json_sql_value *)calloc(pair_count, sizeof(*values));
    if ((pair_count != 0U && keys == NULL) || (pair_count != 0U && values == NULL)) {
        free(keys);
        free(values);
        sqlite3_result_error_nomem(context);
        return MYLITE_NOMEM;
    }

    for (size_t pair_index = 0U; pair_index < pair_count; ++pair_index) {
        size_t key_offset = pair_index * 4U;
        int rc = json_sql_value_from_sqlite(
            context,
            argv[key_offset],
            argv[key_offset + 1U],
            false,
            &keys[pair_index]
        );

        if (rc == MYLITE_OK && keys[pair_index].kind == MYLITE_JSON_SQL_VALUE_NULL) {
            sqlite3_result_error(context, "JSON documents may not contain NULL member names.", -1);
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            rc = json_sql_value_from_sqlite(
                context,
                argv[key_offset + 2U],
                argv[key_offset + 3U],
                true,
                &values[pair_index]
            );
        }
        if (rc != MYLITE_OK) {
            free(values);
            free(keys);
            return rc;
        }
    }

    *out_keys = keys;
    *out_values = values;
    *out_pair_count = pair_count;
    return MYLITE_OK;
}

static int json_sql_value_from_sqlite(
    sqlite3_context *context,
    sqlite3_value *tag_value,
    sqlite3_value *value,
    bool allow_json,
    struct mylite_json_sql_value *out_value
) {
    int64_t kind = 0;

    if (tag_value == NULL || value == NULL || out_value == NULL) {
        sqlite3_result_error(context, "invalid MyLite JSON constructor callback", -1);
        return MYLITE_ERROR;
    }
    *out_value = (struct mylite_json_sql_value){0};
    if (sqlite3_value_type(value) == SQLITE_NULL) {
        out_value->kind = MYLITE_JSON_SQL_VALUE_NULL;
        return MYLITE_OK;
    }
    if (sqlite3_value_type(tag_value) != SQLITE_INTEGER) {
        sqlite3_result_error(context, "invalid MyLite JSON constructor callback", -1);
        return MYLITE_ERROR;
    }

    kind = (int64_t)sqlite3_value_int64(tag_value);
    if (!json_sql_value_kind_is_valid(kind)) {
        sqlite3_result_error(context, "invalid MyLite JSON constructor callback", -1);
        return MYLITE_ERROR;
    }
    out_value->kind = (enum mylite_json_sql_value_kind)kind;
    switch (out_value->kind) {
    case MYLITE_JSON_SQL_VALUE_NULL:
        return MYLITE_OK;
    case MYLITE_JSON_SQL_VALUE_INTEGER:
        if (sqlite3_value_type(value) != SQLITE_INTEGER) {
            sqlite3_result_error(context, "invalid MyLite JSON constructor integer value", -1);
            return MYLITE_ERROR;
        }
        out_value->integer = (int64_t)sqlite3_value_int64(value);
        return MYLITE_OK;
    case MYLITE_JSON_SQL_VALUE_BOOLEAN:
        if (sqlite3_value_type(value) != SQLITE_INTEGER) {
            sqlite3_result_error(context, "invalid MyLite JSON constructor boolean value", -1);
            return MYLITE_ERROR;
        }
        out_value->boolean = sqlite3_value_int64(value) != 0;
        return MYLITE_OK;
    case MYLITE_JSON_SQL_VALUE_STRING:
    case MYLITE_JSON_SQL_VALUE_JSON:
        if (out_value->kind == MYLITE_JSON_SQL_VALUE_JSON && !allow_json) {
            sqlite3_result_error(context, "invalid MyLite JSON_OBJECT key value", -1);
            return MYLITE_ERROR;
        }
        if (sqlite3_value_type(value) != SQLITE_TEXT) {
            sqlite3_result_error(context, "invalid MyLite JSON constructor text value", -1);
            return MYLITE_ERROR;
        }
        out_value->text = (const char *)sqlite3_value_text(value);
        if (out_value->text == NULL) {
            sqlite3_result_error_nomem(context);
            return MYLITE_NOMEM;
        }
        out_value->text_length = (size_t)sqlite3_value_bytes(value);
        return MYLITE_OK;
    }

    sqlite3_result_error(context, "invalid MyLite JSON constructor callback", -1);
    return MYLITE_ERROR;
}

static bool json_sql_value_kind_is_valid(int64_t kind) {
    if (kind < MYLITE_JSON_SQL_VALUE_NULL) {
        return false;
    }
    return kind <= MYLITE_JSON_SQL_VALUE_JSON;
}

static void finish_json_constructor_sqlite_result(
    sqlite3_context *context,
    int rc,
    char *result,
    size_t result_length,
    const struct mylite_json_normalize_result *normalize_result
) {
    if (rc == MYLITE_NOMEM) {
        free(result);
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK) {
        free(result);
        if (normalize_result != NULL &&
            normalize_result->status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
            sqlite3_result_error(context, "Unsupported JSON value in JSON constructor", -1);
        } else if (
            normalize_result != NULL && normalize_result->status == MYLITE_JSON_NORMALIZE_INVALID
        ) {
            sqlite3_result_error(context, "Invalid JSON text in JSON constructor", -1);
        } else {
            sqlite3_result_error(context, "MyLite JSON constructor failed", -1);
        }
        return;
    }
    if (result_length > (size_t)INT_MAX) {
        free(result);
        sqlite3_result_error_nomem(context);
        return;
    }

    sqlite3_result_text(context, result, (int)result_length, free);
}
