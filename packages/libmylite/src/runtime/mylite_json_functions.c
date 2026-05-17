#include "mylite_json_functions.h"

#include <mylite/mylite.h>

#include "mylite_json.h"
#include "mylite_sqlite_registration.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

static void json_extract_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void json_unquote_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void json_valid_sqlite_callback(sqlite3_context *context, int argc, sqlite3_value **argv);

int mylite_sqlite_register_json_functions(sqlite3 *sqlite) {
    static struct mylite_sqlite_function_registration registrations[] = {
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
