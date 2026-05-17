#include "mylite_json_functions.h"

#include <mylite/mylite.h>

#include "mylite_json.h"
#include "mylite_sqlite_registration.h"

#include <stdbool.h>
#include <stddef.h>

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
    };

    return mylite_sqlite_register_functions(
        sqlite,
        registrations,
        sizeof(registrations) / sizeof(registrations[0])
    );
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
