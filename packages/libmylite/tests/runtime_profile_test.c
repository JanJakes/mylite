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
static int test_parser_retry_profile(void);
static int test_cursor_profile(void);
static int test_repeated_prepared_profile(void);
static int test_prepared_plan_cache_profile(void);
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
    failures += test_parser_retry_profile();
    failures += test_cursor_profile();
    failures += test_repeated_prepared_profile();
    failures += test_prepared_plan_cache_profile();
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
    failures += expect_true(snapshot.normalization_count == 1U, "buffered normalization count");
    failures += expect_true(snapshot.parse_count == 1U, "buffered parse count");
    failures += expect_true(snapshot.sqlite_step_count > 0U, "buffered SQLite steps");
    failures += expect_true(snapshot.sqlite_step_ns > 0U, "buffered SQLite step time");
    failures += expect_true(snapshot.metadata_step_count > 0U, "buffered metadata steps");
    failures += expect_true(
        snapshot.metadata_step_count <= snapshot.sqlite_step_count,
        "metadata steps bounded by SQLite steps"
    );
    failures += expect_true(snapshot.metadata_step_ns > 0U, "buffered metadata step time");
    failures += expect_true(
        snapshot.metadata_step_ns <= snapshot.sqlite_step_ns,
        "metadata time bounded by SQLite time"
    );
    failures += expect_true(snapshot.allocation_count > 0U, "buffered allocation count");
    failures += expect_true(snapshot.allocation_bytes > 0U, "buffered allocation bytes");
    failures += expect_true(
        snapshot.execution_statement_cache_miss_count > 0U,
        "buffered execution statement cache misses"
    );
    failures += expect_true(
        snapshot.catalog_statement_cache_hit_count > 0U,
        "buffered catalog statement cache hits"
    );
    failures += expect_true(
        snapshot.parser_retry_callback_count == 0U,
        "buffered successful parse retry count"
    );
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

static int test_parser_retry_profile(void) {
    struct mylite_profile_snapshot snapshot = {0};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open parser profile database");
    failures += expect_int(mylite_profile_start(database), MYLITE_OK, "start parser profile");
    failures += expect_int(
        mylite_execute(database, "SELECT 1 +", strlen("SELECT 1 +"), &result),
        MYLITE_ERROR,
        "profile rejected parser retry query"
    );
    mylite_result_free(result);
    failures += expect_int(
        mylite_profile_stop(database, &snapshot),
        MYLITE_OK,
        "stop rejected parser profile"
    );
    failures += expect_true(snapshot.parse_count == 1U, "rejected parser parse count");
    failures += expect_true(
        snapshot.parser_retry_callback_count > 0U,
        "rejected parser retry callback count"
    );
    failures += expect_true(
        snapshot.parser_retry_handled_count == 0U,
        "rejected parser handled count"
    );

    result = NULL;
    failures += expect_int(mylite_profile_start(database), MYLITE_OK, "restart parser profile");
    failures += expect_int(
        mylite_execute(
            database,
            "SELECT (1, 2) = (1, 2)",
            strlen("SELECT (1, 2) = (1, 2)"),
            &result
        ),
        MYLITE_OK,
        "profile handled parser retry query"
    );
    mylite_result_free(result);
    failures += expect_int(
        mylite_profile_stop(database, &snapshot),
        MYLITE_OK,
        "stop handled parser profile"
    );
    failures += expect_true(
        snapshot.parser_retry_callback_count > 0U,
        "handled parser retry callback count"
    );
    failures += expect_true(
        snapshot.parser_retry_handled_count > 0U,
        "handled parser retry count"
    );
    mylite_close(database);
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
    failures += expect_true(snapshot.normalization_count == 1U, "cursor normalization count");
    failures += expect_true(snapshot.parse_count == 1U, "cursor parse count");
    failures += expect_true(snapshot.select_plan_count == 1U, "cursor plan count");
    failures += expect_true(snapshot.select_plan_cache_hit_count == 0U, "cursor plan hit count");
    failures += expect_true(snapshot.select_lowering_count == 1U, "cursor lowering count");
    failures +=
        expect_true(snapshot.select_lowering_cache_hit_count == 0U, "cursor lowering hit count");
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

static int test_repeated_prepared_profile(void) {
    struct mylite_profile_snapshot snapshot = {0};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    mylite_stmt *dml_stmt = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "prepared") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open repeated prepared profile database"
    );
    failures += expect_int(
        mylite_execute(database, "CREATE DATABASE app", strlen("CREATE DATABASE app"), &result),
        MYLITE_OK,
        "create repeated prepared profile database"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_execute(database, "USE app", strlen("USE app"), &result),
        MYLITE_OK,
        "select repeated prepared profile database"
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
        "create repeated prepared profile table"
    );
    mylite_result_free(result);
    failures += expect_int(
        mylite_prepare(database, "SELECT ? AS value", strlen("SELECT ? AS value"), &stmt),
        MYLITE_OK,
        "prepare repeated profile statement"
    );
    failures += expect_int(
        mylite_prepare(
            database,
            "INSERT INTO t VALUES (?)",
            strlen("INSERT INTO t VALUES (?)"),
            &dml_stmt
        ),
        MYLITE_OK,
        "prepare materialized profile statement"
    );
    failures +=
        expect_int(mylite_profile_start(database), MYLITE_OK, "start repeated prepared profile");
    for (int value = 1; value <= 3; ++value) {
        failures += expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset repeated profile");
        failures += expect_int(
            mylite_stmt_bind_int64(stmt, 0U, value),
            MYLITE_OK,
            "bind repeated profile value"
        );
        failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "step repeated profile row");
        failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "step repeated profile done");
    }
    failures += expect_int(
        mylite_profile_stop(database, &snapshot),
        MYLITE_OK,
        "stop repeated prepared profile"
    );
    failures += expect_true(snapshot.statement_count == 0U, "repeated profile prepare count");
    failures +=
        expect_true(snapshot.normalization_count == 0U, "repeated execution normalization count");
    failures += expect_true(snapshot.parse_count == 0U, "repeated execution parse count");
    failures += expect_true(snapshot.select_plan_count == 3U, "repeated execution plan count");
    failures +=
        expect_true(snapshot.select_plan_cache_hit_count == 0U, "repeated execution plan hits");
    failures += expect_true(snapshot.select_lowering_count == 0U, "repeated lowering count");
    failures += expect_true(
        snapshot.select_lowering_cache_hit_count == 0U,
        "repeated lowering hits"
    );
    failures += expect_true(snapshot.cursor_row_count == 3U, "repeated profile row count");
    failures += expect_true(snapshot.cursor_step_ns > 0U, "repeated profile step time");

    failures += expect_int(
        mylite_profile_start(database),
        MYLITE_OK,
        "start materialized prepared profile"
    );
    failures += expect_int(mylite_stmt_reset(dml_stmt), MYLITE_OK, "reset materialized profile");
    failures += expect_int(
        mylite_stmt_bind_int64(dml_stmt, 0U, 1),
        MYLITE_OK,
        "bind materialized profile value"
    );
    failures +=
        expect_int(mylite_stmt_step(dml_stmt), MYLITE_DONE, "step materialized prepared profile");
    failures += expect_int(
        mylite_profile_stop(database, &snapshot),
        MYLITE_OK,
        "stop materialized prepared profile"
    );
    failures += expect_true(snapshot.cursor_step_ns > 0U, "materialized profile step time");
    failures += expect_true(snapshot.sqlite_step_count > 0U, "materialized profile SQLite steps");
    failures += expect_true(snapshot.descriptor_copy_count == 0U, "materialized descriptor copies");
    failures += expect_true(snapshot.descriptor_copy_bytes == 0U, "materialized descriptor bytes");
    failures += expect_true(snapshot.normalization_count == 0U, "materialized normalization count");
    failures += expect_true(snapshot.parse_count == 0U, "materialized parse count");

    failures +=
        expect_int(mylite_stmt_finalize(dml_stmt), MYLITE_OK, "finalize materialized profile");
    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize repeated profile");
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_prepared_plan_cache_profile(void) {
    struct mylite_profile_snapshot snapshot = {0};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_stmt *stmt = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "plan_cache") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open plan cache database");
    failures += expect_int(
        mylite_execute(database, "CREATE DATABASE app", strlen("CREATE DATABASE app"), &result),
        MYLITE_OK,
        "create plan cache database"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_execute(database, "USE app", strlen("USE app"), &result),
        MYLITE_OK,
        "select plan cache database"
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
        "create plan cache table"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_int(
        mylite_execute(database, "INSERT INTO t VALUES (1)", strlen("INSERT INTO t VALUES (1)"), &result),
        MYLITE_OK,
        "insert plan cache row"
    );
    mylite_result_free(result);
    failures += expect_int(
        mylite_prepare(database, "SELECT id FROM t", strlen("SELECT id FROM t"), &stmt),
        MYLITE_OK,
        "prepare cached plan query"
    );
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "step initial cached plan row");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "step initial cached plan done");

    failures += expect_int(mylite_profile_start(database), MYLITE_OK, "start plan cache profile");
    for (int iteration = 0; iteration < 3; ++iteration) {
        failures += expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset cached plan query");
        failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "step cached plan row");
        failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "step cached plan done");
    }
    failures += expect_int(
        mylite_profile_stop(database, &snapshot),
        MYLITE_OK,
        "stop plan cache profile"
    );
    failures += expect_true(snapshot.select_plan_count == 0U, "cached plan build count");
    failures +=
        expect_true(snapshot.select_plan_cache_hit_count == 3U, "cached plan hit count");
    failures += expect_true(snapshot.select_lowering_count == 0U, "cached lowering build count");
    failures += expect_true(
        snapshot.select_lowering_cache_hit_count == 3U,
        "cached lowering hit count"
    );

    result = NULL;
    failures += expect_int(
        mylite_execute(
            database,
            "ALTER TABLE t ADD COLUMN label VARCHAR(10)",
            strlen("ALTER TABLE t ADD COLUMN label VARCHAR(10)"),
            &result
        ),
        MYLITE_OK,
        "invalidate cached plan schema"
    );
    mylite_result_free(result);
    failures += expect_int(
        mylite_profile_start(database),
        MYLITE_OK,
        "start invalidated plan profile"
    );
    failures += expect_int(mylite_stmt_reset(stmt), MYLITE_OK, "reset invalidated plan query");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_ROW, "step invalidated plan row");
    failures += expect_int(mylite_stmt_step(stmt), MYLITE_DONE, "step invalidated plan done");
    failures += expect_int(
        mylite_profile_stop(database, &snapshot),
        MYLITE_OK,
        "stop invalidated plan profile"
    );
    failures += expect_true(snapshot.select_plan_count == 1U, "invalidated plan build count");
    failures +=
        expect_true(snapshot.select_plan_cache_hit_count == 0U, "invalidated plan hit count");
    failures += expect_true(snapshot.select_lowering_count == 1U, "invalidated lowering count");
    failures += expect_true(
        snapshot.select_lowering_cache_hit_count == 0U,
        "invalidated lowering hit count"
    );

    failures += expect_int(mylite_stmt_finalize(stmt), MYLITE_OK, "finalize cached plan query");
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
