#include <mylite/mylite.h>

#include "runtime/mylite_profile_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    test_path_suffix_capacity = 16,
};

static int test_buffered_profile(void);
static int test_cursor_profile(void);
static int test_connection_attribution(void);
static int test_transaction_control_profile(void);
static int test_close_active_profile(void);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_true(int condition, const char *context);

int main(void) {
    int failures = 0;

    failures += test_buffered_profile();
    failures += test_cursor_profile();
    failures += test_connection_attribution();
    failures += test_transaction_control_profile();
    failures += test_close_active_profile();
    failures +=
        expect_int(mylite_profile_start(NULL), MYLITE_MISUSE, "reject null profile database");

    return failures == 0 ? 0 : 1;
}

static int test_buffered_profile(void) {
    struct mylite_profile_snapshot snapshot = {0};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "buffered") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open buffered database");
    failures += expect_int(
        mylite_execute(database, "CREATE DATABASE app", strlen("CREATE DATABASE app"), &result),
        MYLITE_OK,
        "create buffered profile database"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_execute(database, "USE app", strlen("USE app"), &result),
        MYLITE_OK,
        "select buffered profile database"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_execute(
            database,
            "CREATE TABLE t (id INT)",
            strlen("CREATE TABLE t (id INT)"),
            &result
        ),
        MYLITE_OK,
        "create buffered profile table"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_execute(
            database,
            "INSERT INTO t VALUES (1)",
            strlen("INSERT INTO t VALUES (1)"),
            &result
        ),
        MYLITE_OK,
        "insert buffered profile row"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(mylite_profile_start(database), MYLITE_OK, "start buffered profile");
    failures +=
        expect_int(mylite_profile_start(database), MYLITE_MISUSE, "reject duplicate profile start");
    failures += expect_int(
        mylite_execute(database, "SELECT id FROM t", strlen("SELECT id FROM t"), &result),
        MYLITE_OK,
        "execute buffered profile query"
    );
    failures += expect_true(mylite_result_row_count(result) == 1U, "buffered result rows");
    mylite_result_free(result);
    failures +=
        expect_int(mylite_profile_stop(database, &snapshot), MYLITE_OK, "stop buffered profile");
    failures += expect_true(snapshot.statement_count == 1U, "buffered statement count");
    failures += expect_true(snapshot.statement_api_ns > 0U, "buffered statement time");
    failures += expect_true(snapshot.normalization_ns > 0U, "buffered normalization time");
    failures += expect_true(snapshot.parse_ns > 0U, "buffered parse time");
    failures += expect_true(snapshot.sqlite_step_count > 0U, "buffered SQLite steps");
    failures += expect_true(snapshot.sqlite_step_ns > 0U, "buffered SQLite step time");
    failures += expect_true(snapshot.result_row_count >= 1U, "buffered profiled rows");
    failures += expect_true(snapshot.result_value_bytes >= 1U, "buffered profiled bytes");
    failures += expect_int(
        mylite_profile_stop(database, &snapshot),
        MYLITE_MISUSE,
        "reject duplicate profile stop"
    );
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_cursor_profile(void) {
    struct mylite_profile_snapshot snapshot = {0};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    int failures = 0;
    int rc = MYLITE_OK;

    if (make_test_path(path, sizeof(path), "cursor") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open cursor database");
    {
        mylite_result *result = NULL;

        failures += expect_int(
            mylite_execute(database, "CREATE DATABASE app", strlen("CREATE DATABASE app"), &result),
            MYLITE_OK,
            "create cursor profile database"
        );
        mylite_result_free(result);
        result = NULL;
        failures += expect_int(
            mylite_execute(database, "USE app", strlen("USE app"), &result),
            MYLITE_OK,
            "select cursor profile database"
        );
        mylite_result_free(result);
        result = NULL;
        failures += expect_int(
            mylite_execute(
                database,
                "CREATE TABLE t (id INT)",
                strlen("CREATE TABLE t (id INT)"),
                &result
            ),
            MYLITE_OK,
            "create cursor profile table"
        );
        mylite_result_free(result);
        result = NULL;
        failures += expect_int(
            mylite_execute(
                database,
                "INSERT INTO t VALUES (1)",
                strlen("INSERT INTO t VALUES (1)"),
                &result
            ),
            MYLITE_OK,
            "insert cursor profile row"
        );
        mylite_result_free(result);
    }
    failures += expect_int(mylite_profile_start(database), MYLITE_OK, "start cursor profile");
    failures += expect_int(
        mylite_prepare(database, "SELECT id FROM t", strlen("SELECT id FROM t"), &stmt),
        MYLITE_OK,
        "prepare cursor profile query"
    );
    do {
        rc = mylite_stmt_step(stmt);
    } while (rc == MYLITE_ROW);
    failures += expect_int(rc, MYLITE_DONE, "step cursor profile query");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize cursor profile query");
    failures +=
        expect_int(mylite_profile_stop(database, &snapshot), MYLITE_OK, "stop cursor profile");
    failures += expect_true(snapshot.statement_count == 1U, "cursor statement count");
    failures += expect_true(snapshot.statement_api_ns > 0U, "cursor prepare time");
    failures += expect_true(snapshot.cursor_step_ns > 0U, "cursor step time");
    failures += expect_true(snapshot.cursor_row_count == 1U, "cursor profiled rows");
    failures += expect_true(snapshot.cursor_value_bytes == 1U, "cursor profiled bytes");
    failures += expect_true(snapshot.sqlite_step_count > 0U, "cursor SQLite steps");
    failures += expect_true(snapshot.cursor_finalize_count == 1U, "cursor finalize count");
    failures += expect_true(snapshot.cursor_finalize_ns > 0U, "cursor finalize time");
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_connection_attribution(void) {
    struct mylite_profile_snapshot snapshot = {0};
    mylite_db *profiled_database = NULL;
    mylite_db *other_database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(
        mylite_open_memory(&profiled_database),
        MYLITE_OK,
        "open profiled attribution database"
    );
    failures += expect_int(
        mylite_open_memory(&other_database),
        MYLITE_OK,
        "open other attribution database"
    );
    failures +=
        expect_int(mylite_profile_start(profiled_database), MYLITE_OK, "start attribution profile");
    failures += expect_int(
        mylite_execute(other_database, "SELECT 'other'", strlen("SELECT 'other'"), &result),
        MYLITE_OK,
        "execute unprofiled attribution query"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_profile_stop(other_database, &snapshot),
        MYLITE_MISUSE,
        "reject stopping another connection"
    );
    failures += expect_int(
        mylite_execute(profiled_database, "SELECT 'x'", strlen("SELECT 'x'"), &result),
        MYLITE_OK,
        "execute profiled attribution query"
    );
    mylite_result_free(result);
    failures += expect_int(
        mylite_profile_stop(profiled_database, &snapshot),
        MYLITE_OK,
        "stop attribution profile"
    );
    failures += expect_true(snapshot.statement_count == 1U, "attributed statement count");
    failures += expect_true(snapshot.result_row_count == 1U, "attributed result row count");
    failures += expect_true(snapshot.result_value_bytes == 1U, "attributed result bytes");
    mylite_close(other_database);
    mylite_close(profiled_database);
    return failures;
}

static int test_transaction_control_profile(void) {
    struct mylite_profile_snapshot snapshot = {0};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open transaction database");
    failures += expect_int(mylite_profile_start(database), MYLITE_OK, "start transaction profile");
    failures += expect_int(
        mylite_execute_transaction_control(database, MYLITE_TRANSACTION_CONTROL_START, &result),
        MYLITE_OK,
        "profile start transaction"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_execute_transaction_control(database, MYLITE_TRANSACTION_CONTROL_COMMIT, &result),
        MYLITE_OK,
        "profile commit"
    );
    mylite_result_free(result);
    failures +=
        expect_int(mylite_profile_stop(database, &snapshot), MYLITE_OK, "stop transaction profile");
    failures += expect_true(snapshot.statement_count == 2U, "transaction statement count");
    failures += expect_true(snapshot.statement_api_ns > 0U, "transaction statement time");
    mylite_close(database);
    return failures;
}

static int test_close_active_profile(void) {
    struct mylite_profile_snapshot snapshot = {0};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open close profile database");
    failures += expect_int(mylite_profile_start(database), MYLITE_OK, "start close profile");
    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "reopen profile database");
    failures +=
        expect_int(mylite_profile_start(database), MYLITE_OK, "restart profile after close");
    failures += expect_int(
        mylite_execute(database, "SELECT 1", strlen("SELECT 1"), &result),
        MYLITE_OK,
        "execute restarted profile"
    );
    mylite_result_free(result);
    failures +=
        expect_int(mylite_profile_stop(database, &snapshot), MYLITE_OK, "stop restarted profile");
    failures += expect_true(snapshot.statement_count == 1U, "restarted profile statement count");
    mylite_close(database);
    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *directory = getenv("TMPDIR");
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }
    written = snprintf(
        path,
        path_size,
        "%s/mylite_runtime_profile_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "profile test path is too long for %s\n", name);
        return 1;
    }
    return 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(related_path)) {
        (void)remove(related_path);
    }
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_true(int condition, const char *context) {
    if (condition) {
        return 0;
    }
    fprintf(stderr, "%s: expectation failed\n", context);
    return 1;
}
