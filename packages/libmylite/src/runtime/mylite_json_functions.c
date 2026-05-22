#include "mylite_json_functions.h"

#include <mylite/mylite.h>

#include "mylite_json.h"
#include "mylite_sqlite_registration.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

struct json_search_sqlite_result {
    int rc;
    int64_t contains;
    bool is_null;
    const struct mylite_json_normalize_result *normalize_result;
};

struct json_contains_path_sqlite_arguments {
    const unsigned char *document;
    const char **paths;
    size_t *path_lengths;
    size_t path_count;
    size_t admitted_path_count;
    int document_length;
    bool require_all;
    bool force_null;
};

static void json_array_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void json_object_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void json_contains_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void json_contains_path_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static void json_extract_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void json_length_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void json_type_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
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
static bool json_contains_path_mode_is_all(sqlite3_value *value, bool *out_is_all);
static int decode_json_contains_path_sqlite_mode(
    sqlite3_context *context,
    sqlite3_value *value,
    struct json_contains_path_sqlite_arguments *arguments
);
static int collect_json_contains_path_sqlite_paths(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv,
    struct json_contains_path_sqlite_arguments *arguments
);
static void json_contains_path_sqlite_arguments_deinit(
    struct json_contains_path_sqlite_arguments *arguments
);
static void finish_json_contains_sqlite_result(
    sqlite3_context *context,
    const struct json_search_sqlite_result *result
);
static void finish_json_contains_path_sqlite_result(
    sqlite3_context *context,
    const struct json_search_sqlite_result *result
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
            .name = "_mylite_json_contains",
            .argument_count = -1,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = NULL,
            .scalar_callback = json_contains_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_json_contains_path",
            .argument_count = -1,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = NULL,
            .scalar_callback = json_contains_path_sqlite_callback,
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
            .name = "_mylite_json_length",
            .argument_count = -1,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = NULL,
            .scalar_callback = json_length_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_json_type",
            .argument_count = 1,
            .text_representation =
                SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
            .application_data = NULL,
            .scalar_callback = json_type_sqlite_callback,
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

static void json_contains_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    const unsigned char *target = NULL;
    const unsigned char *candidate = NULL;
    const unsigned char *path = NULL;
    int target_length = 0;
    int candidate_length = 0;
    int path_length = 0;
    int64_t contains = 0;
    bool is_null = false;
    bool force_null = false;
    bool has_path = false;
    struct mylite_json_normalize_result normalize_result = {0};
    struct json_search_sqlite_result result = {0};
    int rc = MYLITE_OK;

    if (context == NULL || (argc != 2 && argc != 3) || argv == NULL || argv[0] == NULL ||
        argv[1] == NULL || (argc == 3 && argv[2] == NULL)) {
        sqlite3_result_error(context, "invalid MyLite JSON_CONTAINS callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT) {
        sqlite3_result_error(context, "Invalid data type for JSON data in JSON_CONTAINS()", -1);
        return;
    }
    target = sqlite3_value_text(argv[0]);
    target_length = sqlite3_value_bytes(argv[0]);
    if (target == NULL || target_length < 0) {
        sqlite3_result_error_nomem(context);
        return;
    }

    if (sqlite3_value_type(argv[1]) == SQLITE_NULL) {
        candidate = (const unsigned char *)"null";
        candidate_length = 4;
        force_null = true;
    } else if (sqlite3_value_type(argv[1]) != SQLITE_TEXT) {
        sqlite3_result_error(context, "Invalid data type for JSON data in JSON_CONTAINS()", -1);
        return;
    } else {
        candidate = sqlite3_value_text(argv[1]);
        candidate_length = sqlite3_value_bytes(argv[1]);
        if (candidate == NULL || candidate_length < 0) {
            sqlite3_result_error_nomem(context);
            return;
        }
    }

    if (!force_null && argc == 3 && sqlite3_value_type(argv[2]) == SQLITE_NULL) {
        force_null = true;
    } else if (!force_null && argc == 3) {
        if (sqlite3_value_type(argv[2]) != SQLITE_TEXT) {
            sqlite3_result_error(context, "Invalid JSON path in JSON_CONTAINS()", -1);
            return;
        }
        path = sqlite3_value_text(argv[2]);
        path_length = sqlite3_value_bytes(argv[2]);
        if (path == NULL || path_length < 0) {
            sqlite3_result_error_nomem(context);
            return;
        }
    }

    has_path = (argc == 3 && !force_null) != 0;
    rc = mylite_json_contains(
        (const char *)target,
        (size_t)target_length,
        (const char *)candidate,
        (size_t)candidate_length,
        (const char *)path,
        (size_t)path_length,
        has_path,
        &contains,
        &is_null,
        &normalize_result
    );
    result.rc = rc;
    result.contains = contains;
    result.is_null = (force_null || is_null) != 0;
    result.normalize_result = &normalize_result;
    finish_json_contains_sqlite_result(context, &result);
}

static void json_contains_path_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    struct json_contains_path_sqlite_arguments arguments = {0};
    struct mylite_json_normalize_result normalize_result = {0};
    struct json_search_sqlite_result result = {0};
    int64_t contains = 0;
    int rc = MYLITE_OK;

    if (context == NULL || argc < 3 || argv == NULL || argv[0] == NULL || argv[1] == NULL) {
        sqlite3_result_error(context, "invalid MyLite JSON_CONTAINS_PATH callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT) {
        sqlite3_result_error(
            context,
            "Invalid data type for JSON data in JSON_CONTAINS_PATH()",
            -1
        );
        return;
    }
    arguments.document = sqlite3_value_text(argv[0]);
    arguments.document_length = sqlite3_value_bytes(argv[0]);
    if (arguments.document == NULL || arguments.document_length < 0) {
        sqlite3_result_error_nomem(context);
        return;
    }

    rc = mylite_json_contains_path(
        (const char *)arguments.document,
        (size_t)arguments.document_length,
        NULL,
        NULL,
        0U,
        false,
        &contains,
        &normalize_result
    );
    if (rc != MYLITE_OK) {
        result.rc = rc;
        result.normalize_result = &normalize_result;
        finish_json_contains_path_sqlite_result(context, &result);
        return;
    }

    if (decode_json_contains_path_sqlite_mode(context, argv[1], &arguments) != MYLITE_OK) {
        return;
    }
    if (collect_json_contains_path_sqlite_paths(context, argc, argv, &arguments) != MYLITE_OK) {
        json_contains_path_sqlite_arguments_deinit(&arguments);
        return;
    }

    rc = mylite_json_contains_path(
        (const char *)arguments.document,
        (size_t)arguments.document_length,
        arguments.paths,
        arguments.path_lengths,
        arguments.admitted_path_count,
        arguments.require_all,
        &contains,
        &normalize_result
    );
    result.rc = rc;
    result.contains = contains;
    result.is_null = arguments.force_null;
    result.normalize_result = &normalize_result;
    finish_json_contains_path_sqlite_result(context, &result);
    json_contains_path_sqlite_arguments_deinit(&arguments);
}

static int decode_json_contains_path_sqlite_mode(
    sqlite3_context *context,
    sqlite3_value *value,
    struct json_contains_path_sqlite_arguments *arguments
) {
    if (context == NULL || value == NULL || arguments == NULL) {
        return MYLITE_MISUSE;
    }
    if (sqlite3_value_type(value) == SQLITE_NULL) {
        arguments->force_null = true;
        return MYLITE_OK;
    }
    if (!json_contains_path_mode_is_all(value, &arguments->require_all)) {
        sqlite3_result_error(context, "Invalid oneOrAll in JSON_CONTAINS_PATH()", -1);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int collect_json_contains_path_sqlite_paths(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv,
    struct json_contains_path_sqlite_arguments *arguments
) {
    if (context == NULL || argv == NULL || arguments == NULL || argc < 3) {
        return MYLITE_MISUSE;
    }
    arguments->path_count = (size_t)argc - 2U;
    if (arguments->path_count > SIZE_MAX / sizeof(*arguments->paths) ||
        arguments->path_count > SIZE_MAX / sizeof(*arguments->path_lengths)) {
        sqlite3_result_error_nomem(context);
        return MYLITE_NOMEM;
    }
    arguments->paths = (const char **)calloc(arguments->path_count, sizeof(*arguments->paths));
    arguments->path_lengths =
        (size_t *)calloc(arguments->path_count, sizeof(*arguments->path_lengths));
    if (arguments->paths == NULL || arguments->path_lengths == NULL) {
        sqlite3_result_error_nomem(context);
        return MYLITE_NOMEM;
    }

    for (size_t path_index = 0U; !arguments->force_null && path_index < arguments->path_count;
         ++path_index) {
        sqlite3_value *path_value = argv[path_index + 2U];
        int sqlite_length = 0;

        if (path_value == NULL) {
            sqlite3_result_error(context, "invalid MyLite JSON_CONTAINS_PATH callback", -1);
            return MYLITE_ERROR;
        }
        if (sqlite3_value_type(path_value) == SQLITE_NULL) {
            arguments->force_null = true;
            break;
        }
        if (sqlite3_value_type(path_value) != SQLITE_TEXT) {
            sqlite3_result_error(context, "Invalid JSON path in JSON_CONTAINS_PATH()", -1);
            return MYLITE_ERROR;
        }
        arguments->paths[path_index] = (const char *)sqlite3_value_text(path_value);
        sqlite_length = sqlite3_value_bytes(path_value);
        if (arguments->paths[path_index] == NULL || sqlite_length < 0) {
            sqlite3_result_error_nomem(context);
            return MYLITE_NOMEM;
        }
        arguments->path_lengths[path_index] = (size_t)sqlite_length;
        arguments->admitted_path_count = path_index + 1U;
    }
    return MYLITE_OK;
}

static void json_contains_path_sqlite_arguments_deinit(
    struct json_contains_path_sqlite_arguments *arguments
) {
    if (arguments == NULL) {
        return;
    }
    free((void *)arguments->paths);
    free(arguments->path_lengths);
    *arguments = (struct json_contains_path_sqlite_arguments){0};
}

static void json_length_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const unsigned char *document = NULL;
    const unsigned char *path = NULL;
    int document_length = 0;
    int path_length = 0;
    int64_t length = 0;
    bool is_null = false;
    bool force_null = false;
    bool has_path = false;
    struct mylite_json_normalize_result normalize_result = {0};
    int rc = MYLITE_OK;

    if (context == NULL || (argc != 1 && argc != 2) || argv == NULL || argv[0] == NULL ||
        (argc == 2 && argv[1] == NULL)) {
        sqlite3_result_error(context, "invalid MyLite JSON_LENGTH callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT) {
        sqlite3_result_error(context, "Invalid data type for JSON data in JSON_LENGTH()", -1);
        return;
    }
    document = sqlite3_value_text(argv[0]);
    document_length = sqlite3_value_bytes(argv[0]);
    if (document == NULL || document_length < 0) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (argc == 2 && sqlite3_value_type(argv[1]) == SQLITE_NULL) {
        force_null = true;
    } else if (argc == 2) {
        if (sqlite3_value_type(argv[1]) != SQLITE_TEXT) {
            sqlite3_result_error(context, "Invalid JSON path in JSON_LENGTH()", -1);
            return;
        }
        path = sqlite3_value_text(argv[1]);
        path_length = sqlite3_value_bytes(argv[1]);
        if (path == NULL || path_length < 0) {
            sqlite3_result_error_nomem(context);
            return;
        }
    }

    has_path = (argc == 2 && !force_null) != 0;
    rc = mylite_json_length(
        (const char *)document,
        (size_t)document_length,
        (const char *)path,
        (size_t)path_length,
        has_path,
        &length,
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
                "Unsupported JSON path or JSON document in JSON_LENGTH()",
                -1
            );
        } else if (normalize_result.status == MYLITE_JSON_NORMALIZE_INVALID) {
            sqlite3_result_error(context, "Invalid JSON text or JSON path in JSON_LENGTH()", -1);
        } else {
            sqlite3_result_error(context, "MyLite JSON_LENGTH failed", -1);
        }
        return;
    }
    if (force_null || is_null) {
        sqlite3_result_null(context);
        return;
    }
    sqlite3_result_int64(context, (sqlite3_int64)length);
}

static void json_type_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const unsigned char *document = NULL;
    const char *type = NULL;
    int document_length = 0;
    struct mylite_json_normalize_result normalize_result = {0};
    int rc = MYLITE_OK;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite JSON_TYPE callback", -1);
        return;
    }
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }
    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT) {
        sqlite3_result_error(context, "Invalid data type for JSON data in JSON_TYPE()", -1);
        return;
    }

    document = sqlite3_value_text(argv[0]);
    document_length = sqlite3_value_bytes(argv[0]);
    if (document == NULL || document_length < 0) {
        sqlite3_result_error_nomem(context);
        return;
    }

    rc =
        mylite_json_type((const char *)document, (size_t)document_length, &type, &normalize_result);
    if (rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (rc != MYLITE_OK || type == NULL) {
        if (normalize_result.status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
            sqlite3_result_error(context, "Unsupported JSON document in JSON_TYPE()", -1);
        } else if (normalize_result.status == MYLITE_JSON_NORMALIZE_INVALID) {
            sqlite3_result_error(context, "Invalid JSON text in JSON_TYPE()", -1);
        } else {
            sqlite3_result_error(context, "MyLite JSON_TYPE failed", -1);
        }
        return;
    }
    sqlite3_result_text(context, type, -1, SQLITE_TRANSIENT);
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

static bool json_contains_path_mode_is_all(sqlite3_value *value, bool *out_is_all) {
    const unsigned char *text = NULL;
    int text_length = 0;
    char first = '\0';
    char second = '\0';
    char third = '\0';

    if (value == NULL || out_is_all == NULL || sqlite3_value_type(value) != SQLITE_TEXT) {
        return false;
    }
    text = sqlite3_value_text(value);
    text_length = sqlite3_value_bytes(value);
    if (text == NULL || text_length < 0) {
        return false;
    }
    if (text_length != 3) {
        return false;
    }

    first = (char)(text[0] >= 'A' && text[0] <= 'Z' ? text[0] + ('a' - 'A') : text[0]);
    second = (char)(text[1] >= 'A' && text[1] <= 'Z' ? text[1] + ('a' - 'A') : text[1]);
    third = (char)(text[2] >= 'A' && text[2] <= 'Z' ? text[2] + ('a' - 'A') : text[2]);
    if (first == 'o' && second == 'n' && third == 'e') {
        *out_is_all = false;
        return true;
    }
    if (first == 'a' && second == 'l' && third == 'l') {
        *out_is_all = true;
        return true;
    }
    return false;
}

static void finish_json_contains_sqlite_result(
    sqlite3_context *context,
    const struct json_search_sqlite_result *result
) {
    if (result == NULL) {
        sqlite3_result_error(context, "MyLite JSON_CONTAINS failed", -1);
        return;
    }
    if (result->rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (result->rc != MYLITE_OK) {
        if (result->normalize_result != NULL &&
            result->normalize_result->status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
            sqlite3_result_error(
                context,
                "Unsupported JSON path or JSON document in JSON_CONTAINS()",
                -1
            );
        } else if (
            result->normalize_result != NULL &&
            result->normalize_result->status == MYLITE_JSON_NORMALIZE_INVALID
        ) {
            sqlite3_result_error(context, "Invalid JSON text or JSON path in JSON_CONTAINS()", -1);
        } else {
            sqlite3_result_error(context, "MyLite JSON_CONTAINS failed", -1);
        }
        return;
    }
    if (result->is_null) {
        sqlite3_result_null(context);
        return;
    }
    sqlite3_result_int64(context, result->contains);
}

static void finish_json_contains_path_sqlite_result(
    sqlite3_context *context,
    const struct json_search_sqlite_result *result
) {
    if (result == NULL) {
        sqlite3_result_error(context, "MyLite JSON_CONTAINS_PATH failed", -1);
        return;
    }
    if (result->rc == MYLITE_NOMEM) {
        sqlite3_result_error_nomem(context);
        return;
    }
    if (result->rc != MYLITE_OK) {
        if (result->normalize_result != NULL &&
            result->normalize_result->status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
            sqlite3_result_error(
                context,
                "Unsupported JSON path or JSON document in JSON_CONTAINS_PATH()",
                -1
            );
        } else if (
            result->normalize_result != NULL &&
            result->normalize_result->status == MYLITE_JSON_NORMALIZE_INVALID
        ) {
            sqlite3_result_error(
                context,
                "Invalid JSON text or JSON path in JSON_CONTAINS_PATH()",
                -1
            );
        } else {
            sqlite3_result_error(context, "MyLite JSON_CONTAINS_PATH failed", -1);
        }
        return;
    }
    if (result->is_null) {
        sqlite3_result_null(context);
        return;
    }
    sqlite3_result_int64(context, result->contains);
}
