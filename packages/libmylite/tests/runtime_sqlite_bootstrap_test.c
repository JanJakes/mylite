#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "runtime/mylite_sqlite_bootstrap.h"
#include "runtime/mylite_sqlite_registration.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum { sqlite_busy_timeout_ms = 5000 };

static int test_bootstrap_policy_on_independent_handles(void);
static int test_callback_owner_lookup_from_scalar_function(void);
static int test_function_registration_surface(void);
static int test_collation_registration_surface(void);
static int test_zero_initialized_bootstrap_cleanup(void);
static int query_db_config_bool(sqlite3 *connection, int operation, int *out_value);
static int query_single_int(sqlite3 *connection, const char *sql, int *out_value);
static int prepare_sql_status(sqlite3 *connection, const char *sql);
static void constant_scalar(sqlite3_context *context, int argc, sqlite3_value **argv);
static void owner_matches_callback(sqlite3_context *context, int argc, sqlite3_value **argv);
static void constant_step(sqlite3_context *context, int argc, sqlite3_value **argv);
static void constant_final(sqlite3_context *context);
static void constant_value(sqlite3_context *context);
static void constant_inverse(sqlite3_context *context, int argc, sqlite3_value **argv);
static int reverse_compare(
    void *application_data,
    int left_size,
    const void *left,
    int right_size,
    const void *right
);
static int expect_int(int actual, int expected, const char *context);
static int expect_bool(bool actual, bool expected, const char *context);
static int expect_true(int condition, const char *context);
static int expect_pointer(const void *actual, const void *expected, const char *context);

int main(void) {
    int failures = 0;

    failures += test_bootstrap_policy_on_independent_handles();
    failures += test_callback_owner_lookup_from_scalar_function();
    failures += test_function_registration_surface();
    failures += test_collation_registration_surface();
    failures += test_zero_initialized_bootstrap_cleanup();

    return failures == 0 ? 0 : 1;
}

static int test_bootstrap_policy_on_independent_handles(void) {
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    sqlite3 *first_sqlite = NULL;
    sqlite3 *second_sqlite = NULL;
    const struct mylite_sqlite_bootstrap_state *first_state = NULL;
    const struct mylite_sqlite_bootstrap_state *second_state = NULL;
    int first_trusted_schema = -1;
    int second_trusted_schema = -1;
    int first_foreign_keys = -1;
    int second_foreign_keys = -1;
    int first_busy_timeout = -1;
    int second_busy_timeout = -1;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second handle");

    first_sqlite = mylite_connection_sqlite_for_test(first);
    second_sqlite = mylite_connection_sqlite_for_test(second);
    first_state = mylite_connection_sqlite_bootstrap_state_for_test(first);
    second_state = mylite_connection_sqlite_bootstrap_state_for_test(second);

    failures += expect_true(first_state != NULL, "first bootstrap state exists");
    failures += expect_true(second_state != NULL, "second bootstrap state exists");
    failures += expect_true(first_state != second_state, "bootstrap state is connection-local");
    if (first_state != NULL) {
        failures += expect_bool(first_state->initialized, true, "first bootstrap initialized");
        failures += expect_bool(
            first_state->owner_client_data_is_registered,
            true,
            "first owner client data"
        );
        failures += expect_bool(
            first_state->trusted_schema_policy_is_applied,
            true,
            "first trusted-schema policy applied"
        );
        failures +=
            expect_bool(first_state->trusted_schema_is_enabled, false, "first trusted schema");
        failures += expect_bool(
            first_state->foreign_key_policy_is_applied,
            true,
            "first foreign-key policy applied"
        );
        failures += expect_bool(
            first_state->foreign_key_enforcement_is_enabled,
            false,
            "first foreign-key enforcement"
        );
        failures += expect_bool(
            first_state->foreign_key_policy_is_placeholder,
            true,
            "first foreign-key placeholder"
        );
        failures += expect_bool(
            first_state->function_registration_surface_is_initialized,
            true,
            "first function surface"
        );
        failures += expect_bool(
            first_state->collation_registration_surface_is_initialized,
            true,
            "first collation surface"
        );
        failures += expect_bool(
            first_state->hooks.registration_surface_is_initialized,
            true,
            "first hook surface"
        );
        failures +=
            expect_bool(first_state->hooks.busy_handler_is_registered, true, "first busy handler");
    }
    if (second_state != NULL) {
        failures += expect_bool(second_state->initialized, true, "second bootstrap initialized");
        failures +=
            expect_bool(second_state->trusted_schema_is_enabled, false, "second trusted schema");
        failures += expect_bool(
            second_state->foreign_key_enforcement_is_enabled,
            false,
            "second foreign-key enforcement"
        );
        failures += expect_bool(
            second_state->foreign_key_policy_is_placeholder,
            true,
            "second foreign-key placeholder"
        );
        failures += expect_bool(
            second_state->hooks.busy_handler_is_registered,
            true,
            "second busy handler"
        );
    }

    failures += expect_pointer(
        mylite_sqlite_bootstrap_owner_from_connection(first_sqlite),
        first,
        "first owner lookup"
    );
    failures += expect_pointer(
        mylite_sqlite_bootstrap_owner_from_connection(second_sqlite),
        second,
        "second owner lookup"
    );
    failures +=
        query_db_config_bool(first_sqlite, SQLITE_DBCONFIG_TRUSTED_SCHEMA, &first_trusted_schema);
    failures +=
        query_db_config_bool(second_sqlite, SQLITE_DBCONFIG_TRUSTED_SCHEMA, &second_trusted_schema);
    failures +=
        query_db_config_bool(first_sqlite, SQLITE_DBCONFIG_ENABLE_FKEY, &first_foreign_keys);
    failures +=
        query_db_config_bool(second_sqlite, SQLITE_DBCONFIG_ENABLE_FKEY, &second_foreign_keys);
    failures += expect_int(first_trusted_schema, 0, "first SQLite trusted-schema setting");
    failures += expect_int(second_trusted_schema, 0, "second SQLite trusted-schema setting");
    failures += expect_int(first_foreign_keys, 0, "first SQLite foreign-key setting");
    failures += expect_int(second_foreign_keys, 0, "second SQLite foreign-key setting");
    failures += query_single_int(first_sqlite, "PRAGMA busy_timeout", &first_busy_timeout);
    failures += query_single_int(second_sqlite, "PRAGMA busy_timeout", &second_busy_timeout);
    failures += expect_int(first_busy_timeout, sqlite_busy_timeout_ms, "first SQLite busy timeout");
    failures +=
        expect_int(second_busy_timeout, sqlite_busy_timeout_ms, "second SQLite busy timeout");

    mylite_close(second);
    mylite_close(first);

    return failures;
}

static int test_callback_owner_lookup_from_scalar_function(void) {
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int callback_result = 0;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open callback handle");
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += expect_int(
        mylite_sqlite_register_functions(
            sqlite,
            &(struct mylite_sqlite_function_registration){
                .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
                .name = "mylite_test_owner_matches",
                .argument_count = 0,
                .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY | SQLITE_INNOCUOUS,
                .application_data = database,
                .scalar_callback = owner_matches_callback,
                .step_callback = NULL,
                .final_callback = NULL,
                .value_callback = NULL,
                .inverse_callback = NULL,
                .destroy_callback = NULL,
            },
            1U
        ),
        MYLITE_OK,
        "register owner callback"
    );
    failures += query_single_int(sqlite, "SELECT mylite_test_owner_matches()", &callback_result);
    failures += expect_int(callback_result, 1, "callback sees owning MyLite handle");

    mylite_close(database);

    return failures;
}

static int test_function_registration_surface(void) {
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    struct mylite_sqlite_function_registration partial_registration[] = {
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "mylite_test_partial_scalar",
            .argument_count = 0,
            .text_representation = SQLITE_UTF8,
            .application_data = NULL,
            .scalar_callback = constant_scalar,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
        {
            .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
            .name = "mylite_test_invalid_partial_scalar",
            .argument_count = 0,
            .text_representation = SQLITE_UTF8,
            .application_data = NULL,
            .scalar_callback = NULL,
            .step_callback = NULL,
            .final_callback = NULL,
            .value_callback = NULL,
            .inverse_callback = NULL,
            .destroy_callback = NULL,
        },
    };
    int aggregate_result = 0;
    int window_result = 0;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open function handle");
    sqlite = mylite_connection_sqlite_for_test(database);

    failures += expect_int(
        mylite_sqlite_register_functions(sqlite, NULL, 0U),
        MYLITE_OK,
        "register empty function list"
    );
    failures += expect_int(
        mylite_sqlite_register_functions(NULL, NULL, 0U),
        MYLITE_MISUSE,
        "function registration rejects NULL SQLite"
    );
    failures += expect_int(
        mylite_sqlite_register_functions(sqlite, NULL, 1U),
        MYLITE_MISUSE,
        "function registration rejects missing descriptors"
    );
    failures += expect_int(
        mylite_sqlite_register_functions(
            sqlite,
            &(struct mylite_sqlite_function_registration){
                .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
                .name = "mylite_test_invalid_scalar",
                .argument_count = 0,
                .text_representation = SQLITE_UTF8,
                .application_data = NULL,
                .scalar_callback = NULL,
                .step_callback = NULL,
                .final_callback = NULL,
                .value_callback = NULL,
                .inverse_callback = NULL,
                .destroy_callback = NULL,
            },
            1U
        ),
        MYLITE_MISUSE,
        "function registration rejects invalid scalar descriptor"
    );
    failures += expect_int(
        mylite_sqlite_register_functions(
            sqlite,
            &(struct mylite_sqlite_function_registration){
                .kind = MYLITE_SQLITE_FUNCTION_SCALAR,
                .name = "mylite_test_invalid_encoding",
                .argument_count = 0,
                .text_representation = SQLITE_DIRECTONLY,
                .application_data = NULL,
                .scalar_callback = constant_scalar,
                .step_callback = NULL,
                .final_callback = NULL,
                .value_callback = NULL,
                .inverse_callback = NULL,
                .destroy_callback = NULL,
            },
            1U
        ),
        MYLITE_MISUSE,
        "function registration rejects missing base encoding"
    );
    failures += expect_int(
        mylite_sqlite_register_functions(sqlite, partial_registration, 2U),
        MYLITE_MISUSE,
        "function registration prevalidates descriptor list"
    );
    failures += expect_int(
        prepare_sql_status(sqlite, "SELECT mylite_test_partial_scalar()"),
        SQLITE_ERROR,
        "invalid function list leaves earlier descriptor unregistered"
    );
    failures += expect_int(
        mylite_sqlite_register_functions(
            sqlite,
            &(struct mylite_sqlite_function_registration){
                .kind = MYLITE_SQLITE_FUNCTION_AGGREGATE,
                .name = "mylite_test_constant_aggregate",
                .argument_count = 1,
                .text_representation = SQLITE_UTF8,
                .application_data = NULL,
                .scalar_callback = NULL,
                .step_callback = constant_step,
                .final_callback = constant_final,
                .value_callback = NULL,
                .inverse_callback = NULL,
                .destroy_callback = NULL,
            },
            1U
        ),
        MYLITE_OK,
        "register aggregate function"
    );
    failures +=
        query_single_int(sqlite, "SELECT mylite_test_constant_aggregate(1)", &aggregate_result);
    failures += expect_int(aggregate_result, 1, "aggregate function result");

    failures += expect_int(
        mylite_sqlite_register_functions(
            sqlite,
            &(struct mylite_sqlite_function_registration){
                .kind = MYLITE_SQLITE_FUNCTION_WINDOW,
                .name = "mylite_test_constant_window",
                .argument_count = 1,
                .text_representation = SQLITE_UTF8,
                .application_data = NULL,
                .scalar_callback = NULL,
                .step_callback = constant_step,
                .final_callback = constant_final,
                .value_callback = constant_value,
                .inverse_callback = constant_inverse,
                .destroy_callback = NULL,
            },
            1U
        ),
        MYLITE_OK,
        "register window function"
    );
    failures +=
        query_single_int(sqlite, "SELECT mylite_test_constant_window(1) OVER ()", &window_result);
    failures += expect_int(window_result, 1, "window function result");

    mylite_close(database);

    return failures;
}

static int test_collation_registration_surface(void) {
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    struct mylite_sqlite_collation_registration partial_registration[] = {
        {
            .name = "mylite_test_partial_collation",
            .text_representation = SQLITE_UTF8,
            .application_data = NULL,
            .compare_callback = reverse_compare,
            .destroy_callback = NULL,
        },
        {
            .name = "mylite_test_invalid_partial_collation",
            .text_representation = SQLITE_UTF8,
            .application_data = NULL,
            .compare_callback = NULL,
            .destroy_callback = NULL,
        },
    };
    int collation_result = 0;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open collation handle");
    sqlite = mylite_connection_sqlite_for_test(database);

    failures += expect_int(
        mylite_sqlite_register_collations(sqlite, NULL, 0U),
        MYLITE_OK,
        "register empty collation list"
    );
    failures += expect_int(
        mylite_sqlite_register_collations(NULL, NULL, 0U),
        MYLITE_MISUSE,
        "collation registration rejects NULL SQLite"
    );
    failures += expect_int(
        mylite_sqlite_register_collations(sqlite, NULL, 1U),
        MYLITE_MISUSE,
        "collation registration rejects missing descriptors"
    );
    failures += expect_int(
        mylite_sqlite_register_collations(
            sqlite,
            &(struct mylite_sqlite_collation_registration){
                .name = "",
                .text_representation = SQLITE_UTF8,
                .application_data = NULL,
                .compare_callback = reverse_compare,
                .destroy_callback = NULL,
            },
            1U
        ),
        MYLITE_MISUSE,
        "collation registration rejects empty name"
    );
    failures += expect_int(
        mylite_sqlite_register_collations(
            sqlite,
            &(struct mylite_sqlite_collation_registration){
                .name = "mylite_test_invalid_collation_flags",
                .text_representation = SQLITE_UTF8 | SQLITE_DIRECTONLY,
                .application_data = NULL,
                .compare_callback = reverse_compare,
                .destroy_callback = NULL,
            },
            1U
        ),
        MYLITE_MISUSE,
        "collation registration rejects function flags"
    );
    failures += expect_int(
        mylite_sqlite_register_collations(sqlite, partial_registration, 2U),
        MYLITE_MISUSE,
        "collation registration prevalidates descriptor list"
    );
    failures += expect_int(
        prepare_sql_status(sqlite, "SELECT 'a' COLLATE mylite_test_partial_collation = 'a'"),
        SQLITE_ERROR,
        "invalid collation list leaves earlier descriptor unregistered"
    );
    failures += expect_int(
        mylite_sqlite_register_collations(
            sqlite,
            &(struct mylite_sqlite_collation_registration){
                .name = "mylite_test_reverse",
                .text_representation = SQLITE_UTF8,
                .application_data = NULL,
                .compare_callback = reverse_compare,
                .destroy_callback = NULL,
            },
            1U
        ),
        MYLITE_OK,
        "register test collation"
    );
    failures += query_single_int(
        sqlite,
        "SELECT CASE WHEN 'a' COLLATE mylite_test_reverse > "
        "'b' COLLATE mylite_test_reverse THEN 1 ELSE 0 END",
        &collation_result
    );
    failures += expect_int(collation_result, 1, "test collation affects comparison");

    mylite_close(database);

    return failures;
}

static int test_zero_initialized_bootstrap_cleanup(void) {
    struct mylite_sqlite_bootstrap_state state;
    int failures = 0;

    memset(&state, 0, sizeof(state));
    mylite_sqlite_bootstrap_deinit(NULL, NULL);
    mylite_sqlite_bootstrap_deinit(NULL, &state);
    failures += expect_bool(state.initialized, false, "zero bootstrap remains uninitialized");
    failures += expect_bool(
        state.owner_client_data_is_registered,
        false,
        "zero bootstrap owner client data"
    );

    return failures;
}

static int query_db_config_bool(sqlite3 *connection, int operation, int *out_value) {
    int value = -1;
    int rc = SQLITE_OK;

    *out_value = -1;
    rc = sqlite3_db_config(connection, operation, -1, &value);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "query SQLite db config %d: SQLite error %d\n", operation, rc);
        return 1;
    }

    *out_value = value;

    return 0;
}

static int query_single_int(sqlite3 *connection, const char *sql, int *out_value) {
    enum { sqlite_use_nul_terminated_string = -1 };

    sqlite3_stmt *statement = NULL;
    int rc = SQLITE_OK;

    *out_value = 0;
    rc = sqlite3_prepare_v2(connection, sql, sqlite_use_nul_terminated_string, &statement, NULL);
    if (rc != SQLITE_OK) {
        fprintf(
            stderr,
            "prepare SQLite SQL \"%s\": error %d: %s\n",
            sql,
            rc,
            sqlite3_errmsg(connection)
        );
        return 1;
    }

    rc = sqlite3_step(statement);
    if (rc != SQLITE_ROW) {
        fprintf(
            stderr,
            "step SQLite SQL \"%s\": error %d: %s\n",
            sql,
            rc,
            sqlite3_errmsg(connection)
        );
        sqlite3_finalize(statement);
        return 1;
    }
    *out_value = sqlite3_column_int(statement, 0);

    rc = sqlite3_finalize(statement);
    if (rc != SQLITE_OK) {
        fprintf(
            stderr,
            "finalize SQLite SQL \"%s\": error %d: %s\n",
            sql,
            rc,
            sqlite3_errmsg(connection)
        );
        return 1;
    }

    return 0;
}

static int prepare_sql_status(sqlite3 *connection, const char *sql) {
    enum { sqlite_use_nul_terminated_string = -1 };

    sqlite3_stmt *statement = NULL;
    int rc =
        sqlite3_prepare_v2(connection, sql, sqlite_use_nul_terminated_string, &statement, NULL);

    if (statement != NULL) {
        (void)sqlite3_finalize(statement);
    }

    return rc;
}

static void constant_scalar(sqlite3_context *context, int argc, sqlite3_value **argv) {
    (void)argc;
    (void)argv;

    sqlite3_result_int(context, 1);
}

static void owner_matches_callback(sqlite3_context *context, int argc, sqlite3_value **argv) {
    mylite_db *expected = sqlite3_user_data(context);
    mylite_db *actual = mylite_sqlite_bootstrap_owner_from_context(context);

    (void)argc;
    (void)argv;

    sqlite3_result_int(context, actual == expected ? 1 : 0);
}

static void constant_step(sqlite3_context *context, int argc, sqlite3_value **argv) {
    (void)context;
    (void)argc;
    (void)argv;
}

static void constant_final(sqlite3_context *context) {
    sqlite3_result_int(context, 1);
}

static void constant_value(sqlite3_context *context) {
    sqlite3_result_int(context, 1);
}

static void constant_inverse(sqlite3_context *context, int argc, sqlite3_value **argv) {
    (void)context;
    (void)argc;
    (void)argv;
}

static int reverse_compare(
    void *application_data,
    int left_size,
    const void *left,
    int right_size,
    const void *right
) {
    int common_size = left_size < right_size ? left_size : right_size;
    int result = memcmp(left, right, (size_t)common_size);

    (void)application_data;

    if (result < 0) {
        return 1;
    }
    if (result > 0) {
        return -1;
    }
    if (left_size < right_size) {
        return 1;
    }
    if (left_size > right_size) {
        return -1;
    }

    return 0;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_bool(bool actual, bool expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, (int)expected, (int)actual);
        return 1;
    }

    return 0;
}

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}

static int expect_pointer(const void *actual, const void *expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %p, got %p\n", context, expected, actual);
        return 1;
    }

    return 0;
}
