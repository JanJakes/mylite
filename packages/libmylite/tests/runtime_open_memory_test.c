#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int test_open_memory_success_and_independent_handles(void);
static int test_open_memory_rejects_null_output(void);
static int test_close_null_is_noop(void);
static int expect_int(int actual, int expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_bool(bool actual, bool expected, const char *context);
static int expect_true(int condition, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);

int main(void) {
    int failures = 0;

    failures += test_open_memory_success_and_independent_handles();
    failures += test_open_memory_rejects_null_output();
    failures += test_close_null_is_noop();

    return failures == 0 ? 0 : 1;
}

static int test_open_memory_success_and_independent_handles(void) {
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    const struct mylite_session_state *session = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first handle");
    failures += expect_true(first != NULL, "first handle is non-null");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second handle");
    failures += expect_true(second != NULL, "second handle is non-null");
    failures += expect_true(first != second, "handles are distinct");

    session = mylite_connection_session_state(first);
    failures += expect_true(session != NULL, "session state exists");
    if (session != NULL) {
        failures += expect_bool(session->has_selected_schema, false, "selected schema is unset");
        failures += expect_text(session->selected_schema, "", "selected schema text");
        failures += expect_text(session->current_user_identity, "root@%", "current user");
        failures += expect_text(session->client_user_identity, "root@%", "client user");
        failures += expect_uint64(
            session->sql_mode,
            MYLITE_SESSION_SQL_MODE_DEFAULT_BITS,
            "SQL mode default value"
        );
        failures += expect_text(
            session->sql_mode_text,
            MYLITE_SESSION_SQL_MODE_DEFAULT_TEXT,
            "SQL mode default text"
        );
        failures += expect_bool(session->sql_mode_is_placeholder, false, "SQL mode is placeholder");
        failures += expect_text(session->time_zone, "SYSTEM", "time zone default text");
        failures += expect_int(session->time_zone_offset_minutes, 0, "time zone offset");
        failures +=
            expect_bool(session->time_zone_is_placeholder, false, "time zone is placeholder");
        failures += expect_bool(
            session->character_set_state_is_placeholder,
            true,
            "character set state is placeholder"
        );
        failures += expect_bool(
            session->system_variables_are_placeholder,
            true,
            "system variables are placeholder"
        );
        failures += expect_uint64(session->catalog_generation, 0U, "catalog generation");
        failures +=
            expect_uint64(session->sqlite_schema_generation, 0U, "SQLite schema generation");
    }

    mylite_close(second);
    mylite_close(first);

    return failures;
}

static int test_open_memory_rejects_null_output(void) {
    return expect_int(mylite_open_memory(NULL), MYLITE_MISUSE, "reject NULL output");
}

static int test_close_null_is_noop(void) {
    mylite_close(NULL);
    return 0;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %" PRIu64 ", got %" PRIu64 "\n", context, expected, actual);
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

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected \"%s\", got \"%s\"\n", context, expected, actual);
        return 1;
    }

    return 0;
}
