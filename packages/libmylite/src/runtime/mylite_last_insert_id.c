#include "mylite_last_insert_id.h"

#include "mylite_connection.h"
#include "mylite_sqlite_bootstrap.h"
#include "mylite_sqlite_registration.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum {
    last_insert_id_uint64_text_capacity = 21,
};

static void last_insert_id_set_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static void last_insert_id_get_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
);
static void set_last_insert_id_sqlite_result(sqlite3_context *context, uint64_t value);

int mylite_sqlite_register_last_insert_id_functions(sqlite3 *sqlite) {
    static const struct mylite_sqlite_function_registration registrations[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_last_insert_id_get",
            .argument_count = 0,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY,
            .application_data = NULL,
            .scalar_callback = last_insert_id_get_sqlite_callback,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "_mylite_last_insert_id_set",
            .argument_count = 1,
            .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY,
            .application_data = NULL,
            .scalar_callback = last_insert_id_set_sqlite_callback,
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

static void last_insert_id_set_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    struct mylite_db *database = NULL;
    int value_type = SQLITE_NULL;
    uint64_t value = 0U;

    if (context == NULL || argc != 1 || argv == NULL || argv[0] == NULL) {
        sqlite3_result_error(context, "invalid MyLite LAST_INSERT_ID callback", -1);
        return;
    }

    database = mylite_sqlite_bootstrap_owner_from_context(context);
    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite LAST_INSERT_ID owner", -1);
        return;
    }

    value_type = sqlite3_value_type(argv[0]);
    if (value_type == SQLITE_NULL) {
        database->session.last_insert_id = 0U;
        sqlite3_result_null(context);
        return;
    }
    if (value_type != SQLITE_INTEGER) {
        sqlite3_result_error(context, "unsupported MyLite LAST_INSERT_ID argument", -1);
        return;
    }

    value = (uint64_t)sqlite3_value_int64(argv[0]);
    database->session.last_insert_id = value;
    set_last_insert_id_sqlite_result(context, value);
}

static void last_insert_id_get_sqlite_callback(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    struct mylite_db *database = NULL;

    (void)argv;
    if (context == NULL || argc != 0) {
        sqlite3_result_error(context, "invalid MyLite LAST_INSERT_ID callback", -1);
        return;
    }

    database = mylite_sqlite_bootstrap_owner_from_context(context);
    if (database == NULL) {
        sqlite3_result_error(context, "missing MyLite LAST_INSERT_ID owner", -1);
        return;
    }
    set_last_insert_id_sqlite_result(context, database->session.last_insert_id);
}

static void set_last_insert_id_sqlite_result(sqlite3_context *context, uint64_t value) {
    char text[last_insert_id_uint64_text_capacity];
    int written = 0;

    if (value <= (uint64_t)INT64_MAX) {
        sqlite3_result_int64(context, (sqlite3_int64)value);
        return;
    }

    written = snprintf(text, sizeof(text), "%" PRIu64, value);
    if (written < 0 || (size_t)written >= sizeof(text)) {
        sqlite3_result_error(context, "failed to format MyLite LAST_INSERT_ID value", -1);
        return;
    }
    sqlite3_result_text(context, text, -1, SQLITE_TRANSIENT);
}
