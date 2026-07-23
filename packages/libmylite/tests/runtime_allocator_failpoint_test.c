#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_test_allocator.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

enum { allocation_sweep_limit = 512 };

static int test_open_failure_is_scoped_and_recoverable(void);
static int test_execute_failure_preserves_handle(void);
static int test_cursor_failure_completes_and_resets(void);
static int expect_true(bool actual, const char *context);

int main(void) {
    int failures = 0;

    failures += test_open_failure_is_scoped_and_recoverable();
    failures += test_cursor_failure_completes_and_resets();
    failures += test_execute_failure_preserves_handle();
    mylite_test_allocator_clear();
    return failures == 0 ? 0 : 1;
}

static int test_open_failure_is_scoped_and_recoverable(void) {
    int failures = 0;
    bool completed_sweep = false;

    for (size_t allocation_index = 0U; allocation_index < allocation_sweep_limit;
         ++allocation_index) {
        mylite_db *database = NULL;
        int rc = MYLITE_OK;

        mylite_test_allocator_fail_after(allocation_index);
        rc = mylite_open_memory(&database);
        if (!mylite_test_allocator_was_triggered()) {
            failures += mylite_test_expect_int(rc, MYLITE_OK, "completed open allocation sweep");
            failures += expect_true(database != NULL, "completed open returns handle");
            mylite_close(database);
            completed_sweep = true;
            mylite_test_allocator_clear();
            break;
        }

        failures += mylite_test_expect_int(rc, MYLITE_NOMEM, "injected open allocation failure");
        failures += expect_true(database == NULL, "failed open leaves output null");
        mylite_test_allocator_clear();
        failures +=
            mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open recovery");
        failures += expect_true(database != NULL, "recovered open returns handle");
        mylite_close(database);
    }

    failures += expect_true(completed_sweep, "open allocation sweep reached success");
    mylite_test_allocator_clear();
    return failures;
}

static int test_cursor_failure_completes_and_resets(void) {
    static const char query[] = "SELECT value FROM items ORDER BY id";
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;
    bool completed_sweep = false;

    failures += mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open cursor sweep handle"
    );
    failures += mylite_test_expect_int(
        mylite_execute(database, "CREATE DATABASE app", strlen("CREATE DATABASE app"), &result),
        MYLITE_OK,
        "create cursor sweep schema"
    );
    mylite_result_free(result);
    result = NULL;
    failures += mylite_test_expect_int(
        mylite_execute(database, "USE app", strlen("USE app"), &result),
        MYLITE_OK,
        "select cursor sweep schema"
    );
    mylite_result_free(result);
    result = NULL;
    failures += mylite_test_expect_int(
        mylite_execute(
            database,
            "CREATE TABLE items (id INT NOT NULL, value VARCHAR(20))",
            strlen("CREATE TABLE items (id INT NOT NULL, value VARCHAR(20))"),
            &result
        ),
        MYLITE_OK,
        "create cursor sweep table"
    );
    mylite_result_free(result);
    result = NULL;
    failures += mylite_test_expect_int(
        mylite_execute(
            database,
            "INSERT INTO items VALUES (1, 'first'), (2, 'second')",
            strlen("INSERT INTO items VALUES (1, 'first'), (2, 'second')"),
            &result
        ),
        MYLITE_OK,
        "seed cursor sweep rows"
    );
    mylite_result_free(result);

    for (size_t allocation_index = 0U; allocation_index < allocation_sweep_limit;
         ++allocation_index) {
        mylite_stmt *stmt = NULL;
        int rc = MYLITE_OK;

        failures += mylite_test_expect_int(
            mylite_prepare(database, query, strlen(query), &stmt),
            MYLITE_OK,
            "prepare cursor allocation sweep"
        );
        mylite_test_allocator_fail_after(allocation_index);
        rc = mylite_stmt_step(stmt);
        if (!mylite_test_allocator_was_triggered()) {
            failures += mylite_test_expect_int(rc, MYLITE_ROW, "completed cursor allocation sweep");
            failures += mylite_test_expect_text(
                mylite_stmt_value_text(stmt, 0U),
                "first",
                "completed cursor retains first row"
            );
            completed_sweep = true;
            mylite_test_allocator_clear();
            failures += mylite_test_expect_int(
                mylite_stmt_finalize(stmt),
                MYLITE_OK,
                "finalize cursor sweep"
            );
            break;
        }

        mylite_test_allocator_clear();
        if (rc == MYLITE_ROW) {
            failures += mylite_test_expect_int(
                mylite_stmt_finalize(stmt),
                MYLITE_OK,
                "finalize tolerated cursor allocation failure"
            );
            continue;
        }
        failures += mylite_test_expect_int(rc, MYLITE_NOMEM, "injected cursor allocation failure");
        failures += mylite_test_expect_int(
            mylite_stmt_step(stmt),
            MYLITE_DONE,
            "failed cursor is completed instead of skipping a row"
        );
        failures +=
            mylite_test_expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset failed cursor");
        failures +=
            mylite_test_expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "reexecute failed cursor");
        failures += mylite_test_expect_text(
            mylite_stmt_value_text(stmt, 0U),
            "first",
            "reset cursor restarts at first row"
        );
        failures += mylite_test_expect_int(
            mylite_stmt_finalize(stmt),
            MYLITE_OK,
            "finalize recovered cursor"
        );
    }

    failures += expect_true(completed_sweep, "cursor allocation sweep reached success");
    mylite_close(database);
    mylite_test_allocator_clear();
    return failures;
}

static int test_execute_failure_preserves_handle(void) {
    static const char query[] = "SELECT CONCAT('allocation', '-', 'sweep')";
    mylite_db *database = NULL;
    int failures = 0;
    bool completed_sweep = false;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open execute handle");
    if (database == NULL) {
        return failures;
    }

    for (size_t allocation_index = 0U; allocation_index < allocation_sweep_limit;
         ++allocation_index) {
        mylite_result *result = NULL;
        int rc = MYLITE_OK;

        mylite_test_allocator_fail_after(allocation_index);
        rc = mylite_execute(database, query, strlen(query), &result);
        if (!mylite_test_allocator_was_triggered()) {
            failures += mylite_test_expect_int(rc, MYLITE_OK, "completed execute allocation sweep");
            failures += expect_true(result != NULL, "completed execute returns result");
            mylite_result_free(result);
            completed_sweep = true;
            mylite_test_allocator_clear();
            break;
        }

        if (rc == MYLITE_OK) {
            failures += expect_true(result != NULL, "tolerated allocation failure returns result");
            mylite_result_free(result);
            mylite_test_allocator_clear();
            continue;
        }

        failures += mylite_test_expect_int(rc, MYLITE_NOMEM, "fatal execute allocation failure");
        failures += expect_true(result == NULL, "fatal execute failure leaves result null");
        mylite_test_allocator_clear();
        failures += mylite_test_expect_int(
            mylite_execute(database, query, strlen(query), &result),
            MYLITE_OK,
            "execute recovery"
        );
        failures += expect_true(result != NULL, "recovered execute returns result");
        mylite_result_free(result);
    }

    failures += expect_true(completed_sweep, "execute allocation sweep reached success");
    mylite_close(database);
    mylite_test_allocator_clear();
    return failures;
}

static int expect_true(bool actual, const char *context) {
    if (!actual) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }
    return 0;
}
