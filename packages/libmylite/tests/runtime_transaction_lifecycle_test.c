#include <mylite/mylite.h>

#include "runtime_test_support.h"

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "sqlite3.h"
#include "storage/mylite_file_format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <pthread.h>
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    mysql_error_parse = 1064,
    mysql_error_unknown = 1105,
    mysql_error_unknown_table = 1051,
    mysql_error_duplicate_key = 1062,
    mysql_error_variable_cant_be_set = 1231,
    mysql_error_savepoint_does_not_exist = 1305,
    mysql_error_transaction_characteristics_changed = 1568,
    mysql_error_read_only_transaction = 1792,
    mysql_warning_consistent_snapshot_ignored = 138,
    transaction_variable_default_scalar_column_count = 10,
    transaction_variable_changed_scalar_column_count = 5,
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

struct expected_nonquery {
    int64_t affected_rows;
    size_t warning_count;
};

struct expected_transaction_control {
    enum mylite_transaction_control_statement statement;
    int64_t affected_rows;
    const char *context;
};

struct rollback_fault_state {
    bool statement_savepoint_started;
    bool rollback_denied;
};

struct commit_fault_state {
    bool statement_savepoint_started;
    bool release_denied;
};

struct catalog_rollback_fault_state {
    bool rollback_denied;
};

#ifndef _WIN32
struct concurrent_write_context {
    mylite_db *database;
    int rc;
    int64_t affected_rows;
};

struct writer_busy_wait_context {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    bool writer_blocked;
    bool reader_released;
};
#endif

static int test_transaction_control_and_dml(void);
static int test_public_transaction_control_api(void);
static int test_set_transaction_lifecycle(void);
static int test_transaction_system_variable_readback(void);
static int test_transaction_system_variable_assignments(void);
static int test_start_transaction_characteristics_lifecycle(void);
static int test_transaction_after_buffered_result_lifecycle(void);
static int test_savepoint_lifecycle(void);
static int test_independent_savepoint_handles(void);
static int test_independent_transaction_characteristic_handles(void);
static int test_read_only_transaction_does_not_reserve_writer_lock(void);
#ifndef _WIN32
static int test_writer_waits_for_active_read_cursor(void);
static void *execute_concurrent_write(void *argument);
static int wait_for_reader_release(void *argument, int previous_attempts);
#endif
static int test_drop_table_missing_implicitly_commits_transaction(void);
static int test_file_close_rolls_back_transaction(void);
static int test_failed_statement_rollback_poisons_connection(void);
static int test_failed_statement_commit_poisons_connection(void);
static int test_failed_catalog_rollback_poisons_connection(void);
static int test_checked_close_reports_cursor_cleanup_failure(void);
static int deny_statement_savepoint_rollback(
    void *context,
    int action,
    const char *first,
    const char *second,
    const char *database_name,
    const char *trigger_name
);
static int deny_statement_savepoint_release(
    void *context,
    int action,
    const char *first,
    const char *second,
    const char *database_name,
    const char *trigger_name
);
static int deny_catalog_transaction_rollback(
    void *context,
    int action,
    const char *first,
    const char *second,
    const char *database_name,
    const char *trigger_name
);
static int seed_schema(mylite_db *database);
static int expect_nonquery(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_transaction_control(
    mylite_db *database,
    struct expected_transaction_control expected
);
static int expect_nonquery_with_warnings(
    mylite_db *database,
    const char *sql,
    struct expected_nonquery expected
);
static int expect_error(mylite_db *database, const char *sql, int expected_code);
static int expect_error_details(
    mylite_db *database,
    const char *sql,
    int expected_code,
    const char *expected_sqlstate,
    const char *expected_message
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_row_count_zero(mylite_db *database, const char *context);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *expected, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_transaction_control_and_dml();
    failures += test_public_transaction_control_api();
    failures += test_set_transaction_lifecycle();
    failures += test_transaction_system_variable_readback();
    failures += test_transaction_system_variable_assignments();
    failures += test_start_transaction_characteristics_lifecycle();
    failures += test_transaction_after_buffered_result_lifecycle();
    failures += test_savepoint_lifecycle();
    failures += test_independent_savepoint_handles();
    failures += test_independent_transaction_characteristic_handles();
    failures += test_read_only_transaction_does_not_reserve_writer_lock();
#ifndef _WIN32
    failures += test_writer_waits_for_active_read_cursor();
#endif
    failures += test_drop_table_missing_implicitly_commits_transaction();
    failures += test_file_close_rolls_back_transaction();
    failures += test_failed_statement_rollback_poisons_connection();
    failures += test_failed_statement_commit_poisons_connection();
    failures += test_failed_catalog_rollback_poisons_connection();
    failures += test_checked_close_reports_cursor_cleanup_failure();

    return failures == 0 ? 0 : 1;
}

static int test_failed_catalog_rollback_poisons_connection(void) {
    struct catalog_rollback_fault_state fault = {0};
    struct mylite_catalog_mutation mutation;
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;
    int rc = MYLITE_OK;

    if (make_test_path(path, sizeof(path), "catalog_rollback_failure") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_catalog_mutation_init(&mutation);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open catalog rollback file");
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += expect_int(
        mylite_catalog_begin_mutation(database, &mutation),
        MYLITE_OK,
        "begin catalog mutation"
    );
    failures += expect_int(
        sqlite3_set_authorizer(sqlite, deny_catalog_transaction_rollback, &fault),
        SQLITE_OK,
        "install catalog rollback authorizer"
    );
    rc = mylite_catalog_rollback_mutation(database, &mutation, MYLITE_ERROR);
    failures += expect_int(rc, MYLITE_ERROR, "failed catalog rollback status");
    failures += expect_int(fault.rollback_denied ? 1 : 0, 1, "catalog rollback denied");
    failures += expect_int(
        database->transaction_state_uncertain ? 1 : 0,
        1,
        "catalog rollback poisons connection"
    );
    failures += expect_contains(
        mylite_errmsg(database),
        "catalog transaction cleanup failed during ROLLBACK",
        "catalog rollback diagnostic"
    );
    failures += expect_int(
        mylite_execute(database, "SELECT 1", strlen("SELECT 1"), &result),
        MYLITE_ERROR,
        "poisoned catalog connection rejects SQL"
    );
    mylite_result_free(result);
    failures += expect_int(
        sqlite3_set_authorizer(sqlite, NULL, NULL),
        SQLITE_OK,
        "remove catalog rollback authorizer"
    );
    failures += expect_int(
        sqlite3_exec(sqlite, "ROLLBACK", NULL, NULL, NULL),
        SQLITE_OK,
        "clean up denied catalog rollback"
    );
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_checked_close_reports_cursor_cleanup_failure(void) {
    struct catalog_rollback_fault_state fault = {0};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_stmt *statement = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "checked_close_cursor_cleanup") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open checked close file");
    failures += seed_schema(database);
    failures += expect_nonquery(database, "CREATE TABLE close_cursor_t (id INT PRIMARY KEY)", 0);
    failures += expect_nonquery(database, "INSERT INTO close_cursor_t VALUES (1), (2)", 2);
    failures += expect_int(
        mylite_prepare(
            database,
            "SELECT id FROM close_cursor_t ORDER BY id",
            strlen("SELECT id FROM close_cursor_t ORDER BY id"),
            &statement
        ),
        MYLITE_OK,
        "prepare checked close cursor"
    );
    failures += expect_int(mylite_stmt_step(statement), MYLITE_ROW, "start checked close cursor");
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += expect_int(
        sqlite3_set_authorizer(sqlite, deny_catalog_transaction_rollback, &fault),
        SQLITE_OK,
        "install checked close rollback authorizer"
    );

    failures += expect_int(
        mylite_close_checked(database),
        MYLITE_ERROR,
        "checked close reports cursor rollback failure"
    );
    failures += expect_int(fault.rollback_denied ? 1 : 0, 1, "checked close rollback denied");
    failures += expect_int(
        database->transaction_state_uncertain ? 1 : 0,
        1,
        "checked close poisons uncertain connection"
    );
    failures += expect_contains(
        mylite_errmsg(database),
        "transaction cleanup failed during ROLLBACK",
        "checked close cleanup diagnostic"
    );
    failures += expect_int(
        mylite_stmt_step(statement),
        MYLITE_MISUSE,
        "checked close detaches cursor after failure"
    );
    failures += expect_int(
        mylite_stmt_finalize(statement),
        MYLITE_OK,
        "finalize cursor detached by checked close"
    );
    statement = NULL;

    failures += expect_int(
        sqlite3_set_authorizer(sqlite, NULL, NULL),
        SQLITE_OK,
        "remove checked close rollback authorizer"
    );
    failures += expect_int(
        sqlite3_exec(sqlite, "ROLLBACK", NULL, NULL, NULL),
        SQLITE_OK,
        "clean up checked close rollback"
    );
    failures += expect_int(
        mylite_close_checked(database),
        MYLITE_OK,
        "retry checked close after cleanup"
    );
    database = NULL;

    remove_related_files(path);
    return failures;
}

static int deny_catalog_transaction_rollback(
    void *context,
    int action,
    const char *first,
    const char *second,
    const char *database_name,
    const char *trigger_name
) {
    struct catalog_rollback_fault_state *fault = context;

    (void)second;
    (void)database_name;
    (void)trigger_name;
    if (action == SQLITE_TRANSACTION && first != NULL && strcmp(first, "ROLLBACK") == 0) {
        fault->rollback_denied = true;
        return SQLITE_DENY;
    }
    return SQLITE_OK;
}

static int test_failed_statement_rollback_poisons_connection(void) {
    static const char *const no_rows[] = {"0"};
    struct rollback_fault_state fault = {0};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;
    int rc = MYLITE_OK;

    if (make_test_path(path, sizeof(path), "rollback_failure") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open rollback failure file");
    failures += seed_schema(database);
    failures += expect_nonquery(database, "CREATE TABLE rollback_t (id INT PRIMARY KEY)", 0);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += expect_int(
        sqlite3_set_authorizer(sqlite, deny_statement_savepoint_rollback, &fault),
        SQLITE_OK,
        "install rollback authorizer"
    );

    rc = mylite_execute(
        database,
        "INSERT INTO rollback_t VALUES (1), (1)",
        strlen("INSERT INTO rollback_t VALUES (1), (1)"),
        &result
    );
    failures += expect_int(rc, MYLITE_ERROR, "failing multi-row insert");
    failures += expect_size((size_t)(result != NULL), 0U, "rollback failure result");
    failures += expect_int(fault.rollback_denied ? 1 : 0, 1, "statement rollback denied");
    failures += expect_int(
        database->transaction_state_uncertain ? 1 : 0,
        1,
        "connection transaction state poisoned"
    );
    failures += expect_contains(
        mylite_errmsg(database),
        "transaction cleanup failed during ROLLBACK TO SAVEPOINT",
        "rollback failure diagnostic context"
    );
    failures += expect_int(
        sqlite3_get_autocommit(sqlite),
        1,
        "emergency rollback restores SQLite autocommit"
    );
    failures += expect_int(
        sqlite3_set_authorizer(sqlite, NULL, NULL),
        SQLITE_OK,
        "remove rollback authorizer"
    );

    rc = mylite_execute(
        database,
        "SELECT COUNT(*) FROM rollback_t",
        strlen("SELECT COUNT(*) FROM rollback_t"),
        &result
    );
    failures += expect_int(rc, MYLITE_ERROR, "poisoned connection rejects SQL");
    failures += expect_int(mylite_errcode(database), mysql_error_unknown, "poisoned error code");
    failures += expect_contains(
        mylite_errmsg(database),
        "close and reopen",
        "poisoned connection diagnostic"
    );
    mylite_result_free(result);
    result = NULL;
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen rollback failure file");
    failures += expect_nonquery(database, "USE app", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM rollback_t",
            .values = no_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "failed statement leaves no committed row",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int deny_statement_savepoint_rollback(
    void *context,
    int action,
    const char *first,
    const char *second,
    const char *database_name,
    const char *trigger_name
) {
    struct rollback_fault_state *fault = context;

    (void)database_name;
    (void)trigger_name;
    if (action != SQLITE_SAVEPOINT || first == NULL || second == NULL ||
        strcmp(second, "_mylite_statement") != 0) {
        return SQLITE_OK;
    }
    if (strcmp(first, "BEGIN") == 0) {
        fault->statement_savepoint_started = true;
        return SQLITE_OK;
    }
    if (fault->statement_savepoint_started && strcmp(first, "ROLLBACK") == 0) {
        fault->rollback_denied = true;
        return SQLITE_DENY;
    }
    return SQLITE_OK;
}

static int test_failed_statement_commit_poisons_connection(void) {
    static const char *const no_rows[] = {"0"};
    struct commit_fault_state fault = {0};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;
    int rc = MYLITE_OK;

    if (make_test_path(path, sizeof(path), "commit_failure") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open commit failure file");
    failures += seed_schema(database);
    failures += expect_nonquery(database, "CREATE TABLE commit_t (id INT PRIMARY KEY)", 0);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += expect_int(
        sqlite3_set_authorizer(sqlite, deny_statement_savepoint_release, &fault),
        SQLITE_OK,
        "install commit authorizer"
    );

    rc = mylite_execute(
        database,
        "INSERT INTO commit_t VALUES (1)",
        strlen("INSERT INTO commit_t VALUES (1)"),
        &result
    );
    failures += expect_int(rc, MYLITE_ERROR, "failing statement commit");
    failures += expect_size((size_t)(result != NULL), 0U, "commit failure result");
    failures += expect_int(fault.release_denied ? 1 : 0, 1, "statement release denied");
    failures += expect_int(
        database->transaction_state_uncertain ? 1 : 0,
        1,
        "commit failure poisons connection"
    );
    failures += expect_contains(
        mylite_errmsg(database),
        "transaction completion failed during RELEASE SAVEPOINT",
        "commit failure diagnostic context"
    );
    failures += expect_int(
        sqlite3_get_autocommit(sqlite),
        1,
        "commit emergency rollback restores SQLite autocommit"
    );
    failures += expect_int(
        sqlite3_set_authorizer(sqlite, NULL, NULL),
        SQLITE_OK,
        "remove commit authorizer"
    );
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen commit failure file");
    failures += expect_nonquery(database, "USE app", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM commit_t",
            .values = no_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "failed statement commit leaves no committed row",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int deny_statement_savepoint_release(
    void *context,
    int action,
    const char *first,
    const char *second,
    const char *database_name,
    const char *trigger_name
) {
    struct commit_fault_state *fault = context;

    (void)database_name;
    (void)trigger_name;
    if (action != SQLITE_SAVEPOINT || first == NULL || second == NULL ||
        strcmp(second, "_mylite_statement") != 0) {
        return SQLITE_OK;
    }
    if (strcmp(first, "BEGIN") == 0) {
        fault->statement_savepoint_started = true;
        return SQLITE_OK;
    }
    if (fault->statement_savepoint_started && strcmp(first, "RELEASE") == 0) {
        fault->release_denied = true;
        return SQLITE_DENY;
    }
    return SQLITE_OK;
}

static int test_transaction_control_and_dml(void) {
    static const char *const one_committed[] = {"1", "10"};
    static const char *const begin_immediate_one[] = {"1", "10"};
    static const char *const begin_immediate_nested[] = {"1", "10", "2", "20"};
    static const char *const nested_rows[] = {"10", "100"};
    static const char *const ddl_rows[] = {"10", "100", "30", "300"};
    static const char *const unique_rows[] = {"1", "2", "3"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "control") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open control file");
    failures += seed_schema(database);
    failures += expect_nonquery(database, "CREATE TABLE t (id INT PRIMARY KEY, v INT)", 0);

    failures += expect_nonquery(database, "COMMIT", 0);
    failures += expect_row_count_zero(database, "commit ROW_COUNT()");
    failures += expect_nonquery(database, "ROLLBACK WORK", 0);
    failures += expect_row_count_zero(database, "rollback work ROW_COUNT()");

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_row_count_zero(database, "start transaction ROW_COUNT()");
    failures += expect_nonquery(database, "INSERT INTO t VALUES (1, 10)", 1);
    failures += expect_nonquery(database, "COMMIT WORK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = one_committed,
            .column_count = 2U,
            .row_count = 1U,
            .context = "committed insert",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (2, 20)", 1);
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = one_committed,
            .column_count = 2U,
            .row_count = 1U,
            .context = "rolled back insert",
        }
    );

    failures += expect_nonquery(database, "BEGIN WORK", 0);
    failures += expect_nonquery(database, "UPDATE t SET v = 11 WHERE id = 1", 1);
    failures += expect_nonquery(database, "ROLLBACK WORK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = one_committed,
            .column_count = 2U,
            .row_count = 1U,
            .context = "rolled back update",
        }
    );

    failures +=
        expect_nonquery(database, "CREATE TABLE begin_immediate_t (id INT PRIMARY KEY, v INT)", 0);
    failures += expect_nonquery(database, "BEGIN IMMEDIATE", 0);
    failures += expect_row_count_zero(database, "begin immediate ROW_COUNT()");
    failures += expect_nonquery(database, "INSERT INTO begin_immediate_t VALUES (1, 10)", 1);
    failures += expect_nonquery(database, "COMMIT", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM begin_immediate_t ORDER BY id",
            .values = begin_immediate_one,
            .column_count = 2U,
            .row_count = 1U,
            .context = "BEGIN IMMEDIATE committed insert",
        }
    );

    failures += expect_nonquery(database, "BEGIN IMMEDIATE", 0);
    failures += expect_nonquery(database, "UPDATE begin_immediate_t SET v = 11 WHERE id = 1", 1);
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM begin_immediate_t ORDER BY id",
            .values = begin_immediate_one,
            .column_count = 2U,
            .row_count = 1U,
            .context = "BEGIN IMMEDIATE rolled back update",
        }
    );

    failures += expect_nonquery(database, "BEGIN IMMEDIATE", 0);
    failures += expect_nonquery(database, "INSERT INTO begin_immediate_t VALUES (2, 20)", 1);
    failures += expect_nonquery(database, "BEGIN IMMEDIATE", 0);
    failures += expect_nonquery(database, "INSERT INTO begin_immediate_t VALUES (3, 30)", 1);
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM begin_immediate_t ORDER BY id",
            .values = begin_immediate_nested,
            .column_count = 2U,
            .row_count = 2U,
            .context = "nested BEGIN IMMEDIATE commits previous transaction",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "DELETE FROM t WHERE id = 1", 1);
    failures += expect_nonquery(database, "COMMIT", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t ORDER BY id",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "committed delete",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (10, 100)", 1);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (20, 200)", 1);
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = nested_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "nested START TRANSACTION commits previous transaction",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (30, 300)", 1);
    failures += expect_nonquery(database, "CREATE TABLE ddl_commit (id INT)", 0);
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = ddl_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "DDL implicitly commits active transaction",
        }
    );
    failures += expect_nonquery(database, "INSERT INTO ddl_commit VALUES (1)", 1);

    failures += expect_nonquery(database, "CREATE TABLE unique_t (id INT PRIMARY KEY)", 0);
    failures += expect_nonquery(database, "INSERT INTO unique_t VALUES (1)", 1);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO unique_t VALUES (2)", 1);
    failures +=
        expect_error(database, "INSERT INTO unique_t VALUES (1)", mysql_error_duplicate_key);
    failures += expect_nonquery(database, "INSERT INTO unique_t VALUES (3)", 1);
    failures += expect_nonquery(database, "COMMIT", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM unique_t ORDER BY id",
            .values = unique_rows,
            .column_count = 1U,
            .row_count = 3U,
            .context = "statement rollback inside active transaction",
        }
    );

    failures += expect_nonquery(database, "CREATE TABLE paths (id INT, v INT)", 0);
    failures += expect_nonquery(database, "CREATE TABLE src (id INT PRIMARY KEY, v INT)", 0);
    failures += expect_nonquery(database, "INSERT INTO src VALUES (3, 30), (5, 50)", 2);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO paths VALUES (1, 10)", 1);
    failures += expect_nonquery(database, "INSERT INTO paths SET id = 2, v = 20", 1);
    failures +=
        expect_nonquery(database, "INSERT INTO paths SELECT id, v FROM src WHERE id = 3", 1);
    failures += expect_nonquery(database, "REPLACE INTO paths VALUES (2, 22)", 1);
    failures += expect_nonquery(database, "REPLACE INTO paths SET id = 4, v = 40", 1);
    failures +=
        expect_nonquery(database, "REPLACE INTO paths SELECT id, v FROM src WHERE id = 5", 1);
    failures += expect_nonquery(database, "UPDATE paths SET v = 99 WHERE id = 1", 1);
    failures += expect_nonquery(database, "DELETE FROM paths WHERE id = 3", 1);
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM paths ORDER BY id",
            .values = NULL,
            .column_count = 1U,
            .row_count = 0U,
            .context = "all DML paths roll back",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_public_transaction_control_api(void) {
    static const char *const committed_rows[] = {"1", "10"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "public_transaction_control") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        expect_int(mylite_open(path, &database), MYLITE_OK, "open public transaction control file");
    failures += seed_schema(database);
    failures += expect_nonquery(database, "CREATE TABLE api_t (id INT PRIMARY KEY, v INT)", 0);

    failures += expect_transaction_control(
        database,
        (struct expected_transaction_control){
            .statement = MYLITE_TRANSACTION_CONTROL_START,
            .affected_rows = 0,
            .context = "public start",
        }
    );
    failures += expect_row_count_zero(database, "public start ROW_COUNT()");
    failures += expect_nonquery(database, "INSERT INTO api_t VALUES (1, 10)", 1);
    failures += expect_transaction_control(
        database,
        (struct expected_transaction_control){
            .statement = MYLITE_TRANSACTION_CONTROL_ROLLBACK,
            .affected_rows = 0,
            .context = "public rollback",
        }
    );
    failures += expect_row_count_zero(database, "public rollback ROW_COUNT()");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM api_t ORDER BY id",
            .values = NULL,
            .column_count = 2U,
            .row_count = 0U,
            .context = "public transaction control rolled back insert",
        }
    );

    failures += expect_transaction_control(
        database,
        (struct expected_transaction_control){
            .statement = MYLITE_TRANSACTION_CONTROL_START,
            .affected_rows = 0,
            .context = "public second start",
        }
    );
    failures += expect_nonquery(database, "INSERT INTO api_t VALUES (1, 10)", 1);
    failures += expect_transaction_control(
        database,
        (struct expected_transaction_control){
            .statement = MYLITE_TRANSACTION_CONTROL_COMMIT,
            .affected_rows = 0,
            .context = "public commit",
        }
    );
    failures += expect_row_count_zero(database, "public commit ROW_COUNT()");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM api_t ORDER BY id",
            .values = committed_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "public transaction control committed insert",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_set_transaction_lifecycle(void) {
    static const char *const one_row[] = {"1", "10"};
    static const char *const two_rows[] = {"1", "10", "2", "20"};
    static const char *const persisted_rows[] = {"1", "10", "2", "20", "6", "60", "8", "80"};
    static const char *const temp_rows[] = {"1"};
    static const char *const ddl_rows[] = {"1"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "set_transaction") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open set transaction file");
    failures += seed_schema(database);
    failures += expect_nonquery(database, "CREATE TABLE t (id INT PRIMARY KEY, v INT)", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (1, 10)", 1);

    failures += expect_nonquery(database, "SET TRANSACTION ISOLATION LEVEL READ COMMITTED", 0);
    failures += expect_row_count_zero(database, "set transaction ROW_COUNT()");
    failures += expect_nonquery(database, "SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED", 0);
    failures += expect_nonquery(database, "SET TRANSACTION ISOLATION LEVEL REPEATABLE READ", 0);
    failures += expect_nonquery(database, "SET TRANSACTION ISOLATION LEVEL SERIALIZABLE", 0);
    failures += expect_nonquery(database, "SET TRANSACTION READ WRITE", 0);
    failures +=
        expect_nonquery(database, "SET TRANSACTION READ WRITE, ISOLATION LEVEL READ COMMITTED", 0);
    failures += expect_nonquery(
        database,
        "SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ, READ WRITE",
        0
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_error_details(
        database,
        "SET TRANSACTION ISOLATION LEVEL SERIALIZABLE",
        mysql_error_transaction_characteristics_changed,
        "25001",
        "Transaction characteristics can't be changed while a transaction is in progress"
    );
    failures += expect_nonquery(database, "ROLLBACK", 0);

    failures += expect_nonquery(database, "SET TRANSACTION READ ONLY", 0);
    failures += expect_error_details(
        database,
        "INSERT INTO t VALUES (2, 20)",
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    failures += expect_error_details(
        database,
        "UPDATE t SET v = 11 WHERE id = 1",
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = one_row,
            .column_count = 2U,
            .row_count = 1U,
            .context = "failed read-only writes do not mutate persistent table",
        }
    );

    failures += expect_nonquery(database, "SET TRANSACTION READ ONLY", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t",
            .values = temp_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "read-only select consumes next transaction characteristic",
        }
    );
    failures += expect_nonquery(database, "INSERT INTO t VALUES (2, 20)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = two_rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "write after select uses session read-write default",
        }
    );

    failures += expect_nonquery(database, "SET TRANSACTION READ ONLY", 0);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_error_details(
        database,
        "DELETE FROM t WHERE id = 1",
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    failures += expect_nonquery(database, "ROLLBACK", 0);

    failures += expect_nonquery(database, "SET TRANSACTION READ ONLY", 0);
    failures += expect_nonquery(database, "BEGIN IMMEDIATE", 0);
    failures += expect_error_details(
        database,
        "INSERT INTO t VALUES (11, 110)",
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    failures += expect_nonquery(database, "ROLLBACK", 0);

    failures += expect_nonquery(database, "CREATE TEMPORARY TABLE tmp (id INT)", 0);
    failures += expect_nonquery(database, "SET TRANSACTION READ ONLY", 0);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO tmp VALUES (1)", 1);
    failures += expect_nonquery(database, "COMMIT", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM tmp ORDER BY id",
            .values = temp_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "temporary DML is allowed in read-only transaction",
        }
    );

    failures += expect_nonquery(database, "SET SESSION TRANSACTION READ ONLY", 0);
    failures += expect_error_details(
        database,
        "INSERT INTO t VALUES (3, 30)",
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    failures += expect_nonquery(database, "SET TRANSACTION READ WRITE", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (6, 60)", 1);
    failures += expect_error_details(
        database,
        "INSERT INTO t VALUES (7, 70)",
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    failures += expect_nonquery(database, "SET SESSION TRANSACTION READ WRITE", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (8, 80)", 1);

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "SET SESSION TRANSACTION READ ONLY", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (9, 90)", 1);
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_error_details(
        database,
        "INSERT INTO t VALUES (10, 100)",
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    failures += expect_nonquery(database, "SET SESSION TRANSACTION READ WRITE", 0);

    failures += expect_nonquery(database, "SET TRANSACTION READ ONLY", 0);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "CREATE TABLE ddl_read_only_marker (id INT)", 0);
    failures += expect_nonquery(database, "INSERT INTO ddl_read_only_marker VALUES (1)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM ddl_read_only_marker",
            .values = ddl_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "DDL implicit commit clears active read-only state",
        }
    );

    failures += expect_error_details(
        database,
        "SET GLOBAL TRANSACTION READ WRITE",
        mysql_error_parse,
        "42000",
        "SET GLOBAL TRANSACTION is not supported"
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = persisted_rows,
            .column_count = 2U,
            .row_count = sizeof(persisted_rows) / (2U * sizeof(persisted_rows[0])),
            .context = "set transaction persistent rows before reopen",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen set transaction file");
    failures += expect_nonquery(database, "USE app", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = persisted_rows,
            .column_count = 2U,
            .row_count = sizeof(persisted_rows) / (2U * sizeof(persisted_rows[0])),
            .context = "set transaction rows persist after reopen",
        }
    );
    mylite_close(database);

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read set transaction preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "set transaction lifecycle preserves MyLite preamble"
    );

    remove_related_files(path);
    return failures;
}

static int test_transaction_system_variable_readback(void) {
    static const char *const default_values[] = {
        "REPEATABLE-READ",
        "REPEATABLE-READ",
        "REPEATABLE-READ",
        "REPEATABLE-READ",
        "0",
        "0",
        "0",
        "0",
        "0",
        "-1",
    };
    static const char *const label_values[] = {
        "REPEATABLE-READ",
        "REPEATABLE-READ",
        "0",
        "0",
    };
    static const char *const show_session_values[] = {
        "transaction_isolation",
        "REPEATABLE-READ",
        "transaction_read_only",
        "OFF",
    };
    static const char *const show_global_values[] = {
        "transaction_isolation",
        "REPEATABLE-READ",
        "transaction_read_only",
        "OFF",
    };
    static const char *const changed_values[] = {
        "READ-COMMITTED",
        "1",
        "REPEATABLE-READ",
        "0",
        "0",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "system_variables") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open transaction vars file");
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@transaction_isolation, @@global.transaction_isolation, "
                   "@@session.transaction_isolation, @@local.transaction_isolation, "
                   "@@transaction_read_only, @@global.transaction_read_only, "
                   "@@session.transaction_read_only, @@local.transaction_read_only, "
                   "@@warning_count, ROW_COUNT()",
            .values = default_values,
            .column_count = transaction_variable_default_scalar_column_count,
            .row_count = 1U,
            .context = "transaction variable default scalar values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@TRANSACTION_ISOLATION, @@Global.`transaction_isolation`, "
                   "@@TRANSACTION_READ_ONLY, @@session.`transaction_read_only`",
            .values = label_values,
            .column_count = 4U,
            .row_count = 1U,
            .context = "transaction variable scalar labels",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW VARIABLES WHERE Variable_name IN "
                   "('transaction_isolation','transaction_read_only')",
            .values = show_session_values,
            .column_count = 2U,
            .row_count = 2U,
            .context = "transaction variable session SHOW rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW GLOBAL VARIABLES WHERE Variable_name IN "
                   "('transaction_isolation','transaction_read_only')",
            .values = show_global_values,
            .column_count = 2U,
            .row_count = 2U,
            .context = "transaction variable global SHOW rows",
        }
    );

    failures +=
        expect_nonquery(database, "SET SESSION transaction_isolation = 'READ-COMMITTED'", 0);
    failures += expect_nonquery(database, "SET transaction_read_only = ON", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@transaction_isolation, @@transaction_read_only, "
                   "@@global.transaction_isolation, @@global.transaction_read_only, ROW_COUNT()",
            .values = changed_values,
            .column_count = transaction_variable_changed_scalar_column_count,
            .row_count = 1U,
            .context = "transaction variable changed session scalar values",
        }
    );

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "transaction variables leave catalog generation"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "transaction variables leave SQLite schema generation"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read transaction vars preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "transaction variables preserve MyLite preamble"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen transaction vars file");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@transaction_isolation, @@global.transaction_isolation, "
                   "@@session.transaction_isolation, @@local.transaction_isolation, "
                   "@@transaction_read_only, @@global.transaction_read_only, "
                   "@@session.transaction_read_only, @@local.transaction_read_only, "
                   "@@warning_count, ROW_COUNT()",
            .values = default_values,
            .column_count = transaction_variable_default_scalar_column_count,
            .row_count = 1U,
            .context = "reopened transaction variable defaults",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_transaction_system_variable_assignments(void) {
    static const char snapshot_warning_message[] =
        "InnoDB: WITH CONSISTENT SNAPSHOT was ignored because this phrase can only be used "
        "with REPEATABLE READ isolation level.";
    static const char *const session_values[] = {"SERIALIZABLE", "1", "0", "0"};
    static const char *const next_read_only_values[] = {"0", "0", "0", "0"};
    static const char *const row_values_after_failed_read_only[] = {"1", "10"};
    static const char *const next_isolation_values[] = {"REPEATABLE-READ", "REPEATABLE-READ", "0"};
    static const char *const snapshot_warning[] = {
        "Warning",
        "138",
        snapshot_warning_message,
    };
    static const char *const active_session_count_values[] = {"3"};
    static const char *const active_session_read_only_values[] = {"1"};
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_test_open_temporary(&database), MYLITE_OK, "open transaction vars");
    failures += seed_schema(database);
    failures += expect_nonquery(database, "CREATE TABLE t (id INT PRIMARY KEY, v INT)", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (1, 10)", 1);

    failures += expect_nonquery(database, "SET transaction_isolation = 'READ-COMMITTED'", 0);
    failures += expect_nonquery(database, "SET transaction_isolation = SERIALIZABLE", 0);
    failures += expect_nonquery(database, "SET transaction_read_only = 'ON'", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@transaction_isolation, @@transaction_read_only, "
                   "@@warning_count, ROW_COUNT()",
            .values = session_values,
            .column_count = 4U,
            .row_count = 1U,
            .context = "transaction variable session assignment",
        }
    );
    failures += expect_nonquery(database, "SET transaction_isolation = DEFAULT", 0);
    failures += expect_nonquery(database, "SET transaction_read_only = DEFAULT", 0);
    failures += expect_nonquery(database, "SET LOCAL transaction_read_only = ON", 0);
    failures += expect_nonquery(database, "SET @@LOCAL.transaction_read_only = OFF", 0);
    failures += expect_nonquery(database, "SET @@SESSION.transaction_read_only = TRUE", 0);
    failures += expect_nonquery(database, "SET transaction_read_only = OFF", 0);

    failures += expect_nonquery(database, "SET @@transaction_read_only = ON", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@transaction_read_only, @@session.transaction_read_only, "
                   "@@warning_count, ROW_COUNT()",
            .values = next_read_only_values,
            .column_count = 4U,
            .row_count = 1U,
            .context = "next transaction read_only scalar unchanged",
        }
    );
    failures += expect_error_details(
        database,
        "INSERT INTO t VALUES (2, 20)",
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = row_values_after_failed_read_only,
            .column_count = 2U,
            .row_count = 1U,
            .context = "next transaction read_only blocks row mutation",
        }
    );
    failures += expect_nonquery(database, "SET SESSION transaction_read_only = OFF", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (2, 20)", 1);

    failures += expect_nonquery(database, "SET @@transaction_isolation = 'READ-COMMITTED'", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@transaction_isolation, @@session.transaction_isolation, ROW_COUNT()",
            .values = next_isolation_values,
            .column_count = 3U,
            .row_count = 1U,
            .context = "next transaction isolation scalar unchanged",
        }
    );
    failures += expect_nonquery_with_warnings(
        database,
        "START TRANSACTION WITH CONSISTENT SNAPSHOT",
        (struct expected_nonquery){0, 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = snapshot_warning,
            .column_count = 3U,
            .row_count = 1U,
            .context = "direct next isolation consistent snapshot warning",
        }
    );
    failures += expect_nonquery(database, "ROLLBACK", 0);

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_error_details(
        database,
        "SET @@transaction_read_only = ON",
        mysql_error_transaction_characteristics_changed,
        "25001",
        "Transaction characteristics can't be changed while a transaction is in progress"
    );
    failures += expect_nonquery(database, "ROLLBACK", 0);

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "SET transaction_read_only = ON", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (3, 30)", 1);
    failures += expect_nonquery(database, "COMMIT", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t",
            .values = active_session_count_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "session assignment inside active transaction count",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@transaction_read_only",
            .values = active_session_read_only_values,
            .column_count = 1U,
            .row_count = 1U,
            .context = "session assignment inside active transaction read_only",
        }
    );
    failures += expect_error_details(
        database,
        "INSERT INTO t VALUES (4, 40)",
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    failures += expect_nonquery(database, "SET transaction_read_only = OFF", 0);

    failures +=
        expect_nonquery(database, "SET GLOBAL transaction_isolation = 'REPEATABLE-READ'", 0);
    failures += expect_nonquery(database, "SET @@GLOBAL.transaction_read_only = OFF", 0);
    failures += expect_error_details(
        database,
        "SET @@GLOBAL.transaction_isolation = 'SERIALIZABLE'",
        mysql_error_parse,
        "42000",
        "SET transaction_isolation supports only fixed no-op global assignments"
    );
    failures += expect_error_details(
        database,
        "SET GLOBAL transaction_read_only = ON",
        mysql_error_parse,
        "42000",
        "SET transaction_read_only supports only fixed no-op global assignments"
    );
    failures += expect_error_details(
        database,
        "SET SESSION transaction_isolation = 'READ COMMITTED'",
        mysql_error_variable_cant_be_set,
        "42000",
        "Variable 'transaction_isolation' can't be set to the value of 'READ COMMITTED'"
    );
    failures += expect_error_details(
        database,
        "SET SESSION transaction_read_only = 2",
        mysql_error_variable_cant_be_set,
        "42000",
        "Variable 'transaction_read_only' can't be set to the value of '2'"
    );
    failures += expect_error_details(
        database,
        "SET SESSION transaction_read_only = '1'",
        mysql_error_variable_cant_be_set,
        "42000",
        "Variable 'transaction_read_only' can't be set to the value of '1'"
    );

    mylite_close(database);
    return failures;
}

static int test_start_transaction_characteristics_lifecycle(void) {
    static const char snapshot_warning_message[] =
        "InnoDB: WITH CONSISTENT SNAPSHOT was ignored because this phrase can only be used "
        "with REPEATABLE READ isolation level.";
    const char *const snapshot_warning[] = {
        "Warning",
        "138",
        snapshot_warning_message,
    };
    static const char *const temp_rows[] = {"1"};
    static const char *const final_rows[] =
        {"1", "10", "2", "20", "3", "30", "4", "40", "5", "50", "6", "60"};
    static const char *const marker_rows[] = {"1"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "start_transaction_characteristics") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open start transaction characteristics file"
    );
    failures += seed_schema(database);
    failures += expect_nonquery(database, "CREATE TABLE t (id INT PRIMARY KEY, v INT)", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (1, 10)", 1);

    failures += expect_nonquery(database, "START TRANSACTION READ ONLY", 0);
    failures += expect_error_details(
        database,
        "INSERT INTO t VALUES (99, 990)",
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    failures += expect_nonquery(database, "ROLLBACK", 0);

    failures += expect_nonquery(database, "START TRANSACTION READ WRITE", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (2, 20)", 1);
    failures += expect_nonquery(database, "COMMIT", 0);

    failures += expect_nonquery(database, "SET SESSION TRANSACTION READ ONLY", 0);
    failures += expect_nonquery(database, "START TRANSACTION READ WRITE", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (3, 30)", 1);
    failures += expect_nonquery(database, "COMMIT", 0);
    failures += expect_error_details(
        database,
        "INSERT INTO t VALUES (98, 980)",
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    failures += expect_nonquery(database, "SET SESSION TRANSACTION READ WRITE", 0);

    failures += expect_nonquery(database, "SET TRANSACTION READ ONLY", 0);
    failures += expect_nonquery(database, "START TRANSACTION WITH CONSISTENT SNAPSHOT", 0);
    failures += expect_error_details(
        database,
        "UPDATE t SET v = 11 WHERE id = 1",
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (4, 40)", 1);

    failures += expect_nonquery(database, "SET TRANSACTION ISOLATION LEVEL READ COMMITTED", 0);
    failures += expect_nonquery_with_warnings(
        database,
        "START TRANSACTION WITH CONSISTENT SNAPSHOT",
        (struct expected_nonquery){0, 1U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = snapshot_warning,
            .column_count = 3U,
            .row_count = 1U,
            .context = "consistent snapshot warning row",
        }
    );
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_nonquery(database, "SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED", 0);
    failures += expect_nonquery_with_warnings(
        database,
        "START TRANSACTION WITH CONSISTENT SNAPSHOT",
        (struct expected_nonquery){0, 1U}
    );
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_nonquery(database, "SET TRANSACTION ISOLATION LEVEL SERIALIZABLE", 0);
    failures += expect_nonquery_with_warnings(
        database,
        "START TRANSACTION WITH CONSISTENT SNAPSHOT",
        (struct expected_nonquery){0, 1U}
    );
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_nonquery_with_warnings(
        database,
        "START TRANSACTION WITH CONSISTENT SNAPSHOT",
        (struct expected_nonquery){0, 0U}
    );
    failures += expect_nonquery(database, "ROLLBACK", 0);

    failures += expect_nonquery(database, "CREATE TEMPORARY TABLE tmp (id INT)", 0);
    failures += expect_nonquery(database, "START TRANSACTION READ ONLY", 0);
    failures += expect_nonquery(database, "INSERT INTO tmp VALUES (1)", 1);
    failures += expect_nonquery(database, "COMMIT", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM tmp ORDER BY id",
            .values = temp_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "start read-only allows temporary DML",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (5, 50)", 1);
    failures += expect_nonquery(database, "SAVEPOINT nested_start_sp", 0);
    failures += expect_nonquery(database, "START TRANSACTION READ ONLY", 0);
    failures +=
        expect_error(database, "ROLLBACK TO nested_start_sp", mysql_error_savepoint_does_not_exist);
    failures += expect_error_details(
        database,
        "INSERT INTO t VALUES (97, 970)",
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    failures += expect_nonquery(database, "ROLLBACK", 0);

    failures +=
        expect_error(database, "START TRANSACTION READ ONLY, READ WRITE", mysql_error_parse);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (6, 60)", 1);

    failures += expect_nonquery(database, "START TRANSACTION READ ONLY", 0);
    failures += expect_nonquery(database, "CREATE TABLE ddl_marker (id INT)", 0);
    failures += expect_nonquery(database, "INSERT INTO ddl_marker VALUES (1)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM ddl_marker",
            .values = marker_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "start read-only DDL implicit commit",
        }
    );

    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = final_rows,
            .column_count = 2U,
            .row_count = sizeof(final_rows) / (2U * sizeof(final_rows[0])),
            .context = "start transaction characteristics final rows",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "reopen start transaction characteristics file"
    );
    failures += expect_nonquery(database, "USE app", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = final_rows,
            .column_count = 2U,
            .row_count = sizeof(final_rows) / (2U * sizeof(final_rows[0])),
            .context = "start transaction characteristics rows persist after reopen",
        }
    );
    mylite_close(database);

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read start transaction characteristics preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "start transaction characteristics preserves MyLite preamble"
    );

    remove_related_files(path);
    return failures;
}

static int test_transaction_after_buffered_result_lifecycle(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *held_result = NULL;
    int failures = 0;
    int rc = MYLITE_OK;

    if (make_test_path(path, sizeof(path), "buffered_result_transaction") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open buffered result file");
    failures += seed_schema(database);
    failures += expect_nonquery(database, "CREATE TABLE t (id INT PRIMARY KEY)", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (1)", 1);

    rc = mylite_execute(
        database,
        "SELECT COUNT(*) FROM t",
        strlen("SELECT COUNT(*) FROM t"),
        &held_result
    );
    failures += expect_int(rc, MYLITE_OK, "buffered result select");
    failures += expect_result_value(held_result, 0U, 0U, "1", "buffered result count");

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (2)", 1);
    failures += expect_nonquery(database, "ROLLBACK", 0);

    mylite_result_free(held_result);
    held_result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM t ORDER BY id",
            .values = (const char *const[]){"1"},
            .column_count = 1U,
            .row_count = 1U,
            .context = "buffered result transaction rollback",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_savepoint_lifecycle(void) {
    static const char *const missing_warning[] = {
        "Error",
        "1305",
        "SAVEPOINT outside_sp does not exist",
    };
    static const char *const first_rollback_rows[] = {"1", "10"};
    static const char *const release_rows[] = {"1", "10", "4", "40", "5", "50"};
    static const char *const replacement_rows[] = {"1"};
    static const char *const case_rows[] = {"3"};
    static const char *const nested_start_rows[] = {"1"};
    static const char *const ddl_rows[] = {"1", "1"};
    static const char *const statement_error_rows[] = {"0"};
    static const char *const reopen_rows[] =
        {"1", "10", "4", "40", "5", "50", "8", "80", "17", "170"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "savepoint") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open savepoint file");
    failures += seed_schema(database);
    failures += expect_nonquery(database, "CREATE TABLE t (id INT PRIMARY KEY, v INT)", 0);

    failures += expect_nonquery(database, "SAVEPOINT outside_sp", 0);
    failures += expect_row_count_zero(database, "savepoint ROW_COUNT()");
    failures += expect_error_details(
        database,
        "ROLLBACK TO SAVEPOINT outside_sp",
        mysql_error_savepoint_does_not_exist,
        "42000",
        "SAVEPOINT outside_sp does not exist"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = missing_warning,
            .column_count = 3U,
            .row_count = 1U,
            .context = "missing savepoint warning row",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (1, 10)", 1);
    failures += expect_nonquery(database, "SAVEPOINT a", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (2, 20)", 1);
    failures += expect_nonquery(database, "SAVEPOINT b", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (3, 30)", 1);
    failures += expect_nonquery(database, "ROLLBACK TO SAVEPOINT a", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = first_rollback_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "rollback to savepoint keeps target",
        }
    );
    failures += expect_nonquery(database, "ROLLBACK TO a", 0);
    failures += expect_error(database, "ROLLBACK TO b", mysql_error_savepoint_does_not_exist);
    failures += expect_nonquery(database, "RELEASE SAVEPOINT a", 0);
    failures += expect_error(database, "RELEASE SAVEPOINT a", mysql_error_savepoint_does_not_exist);
    failures += expect_nonquery(database, "COMMIT", 0);

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (4, 40)", 1);
    failures += expect_nonquery(database, "SAVEPOINT released", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (5, 50)", 1);
    failures += expect_nonquery(database, "RELEASE SAVEPOINT released", 0);
    failures += expect_nonquery(database, "COMMIT", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = release_rows,
            .column_count = 2U,
            .row_count = 3U,
            .context = "release keeps row changes",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "SAVEPOINT dup_a", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (14, 140)", 1);
    failures += expect_nonquery(database, "SAVEPOINT dup_b", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (15, 150)", 1);
    failures += expect_nonquery(database, "SAVEPOINT dup_a", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (16, 160)", 1);
    failures += expect_nonquery(database, "ROLLBACK WORK TO SAVEPOINT dup_b", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t WHERE id IN (14, 15, 16)",
            .values = replacement_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "replacement preserves different later savepoint",
        }
    );
    failures += expect_nonquery(database, "ROLLBACK", 0);

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "SAVEPOINT dup", 0);
    failures += expect_nonquery(database, "SAVEPOINT dup", 0);
    failures += expect_nonquery(database, "RELEASE SAVEPOINT dup", 0);
    failures +=
        expect_error(database, "ROLLBACK TO SAVEPOINT dup", mysql_error_savepoint_does_not_exist);
    failures += expect_nonquery(database, "ROLLBACK", 0);

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "SAVEPOINT MixedName", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (6, 60)", 1);
    failures += expect_nonquery(database, "ROLLBACK TO mixedname", 0);
    failures += expect_nonquery(database, "SAVEPOINT `sp ace`", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (7, 70)", 1);
    failures += expect_nonquery(database, "ROLLBACK TO SAVEPOINT `SP ACE`", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t",
            .values = case_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "case-insensitive savepoint names",
        }
    );
    failures += expect_nonquery(database, "COMMIT", 0);

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (17, 170)", 1);
    failures += expect_nonquery(database, "SAVEPOINT nested_start_sp", 0);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures +=
        expect_error(database, "ROLLBACK TO nested_start_sp", mysql_error_savepoint_does_not_exist);
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t WHERE id = 17",
            .values = nested_start_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "nested START TRANSACTION clears savepoint and commits previous work",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (8, 80)", 1);
    failures += expect_nonquery(database, "SAVEPOINT ddl_sp", 0);
    failures += expect_nonquery(database, "CREATE TABLE ddl_savepoint_marker (id INT)", 0);
    failures += expect_error(database, "ROLLBACK TO ddl_sp", mysql_error_savepoint_does_not_exist);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t WHERE id = 8",
            .values = &ddl_rows[0],
            .column_count = 1U,
            .row_count = 1U,
            .context = "DDL savepoint row persisted",
        }
    );
    failures += expect_nonquery(database, "INSERT INTO ddl_savepoint_marker VALUES (1)", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM ddl_savepoint_marker",
            .values = &ddl_rows[1],
            .column_count = 1U,
            .row_count = 1U,
            .context = "DDL savepoint table persisted",
        }
    );

    failures += expect_nonquery(database, "CREATE TABLE unique_savepoint (id INT PRIMARY KEY)", 0);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "SAVEPOINT before_error", 0);
    failures += expect_nonquery(database, "INSERT INTO unique_savepoint VALUES (1)", 1);
    failures += expect_error(
        database,
        "INSERT INTO unique_savepoint VALUES (1)",
        mysql_error_duplicate_key
    );
    failures += expect_nonquery(database, "ROLLBACK TO before_error", 0);
    failures += expect_nonquery(database, "COMMIT", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM unique_savepoint",
            .values = statement_error_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "statement error preserves user savepoint",
        }
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen savepoint file");
    failures += expect_nonquery(database, "USE app", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = reopen_rows,
            .column_count = 2U,
            .row_count = sizeof(reopen_rows) / (2U * sizeof(reopen_rows[0])),
            .context = "committed savepoint-controlled changes persist after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_savepoint_handles(void) {
    static const char *const first_handle_rows[] = {"1"};
    static const char *const second_handle_rows[] = {"10"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "savepoint_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "savepoint_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += seed_schema(first);
    failures += seed_schema(second);
    failures += expect_nonquery(first, "CREATE TABLE t (id INT PRIMARY KEY)", 0);
    failures += expect_nonquery(second, "CREATE TABLE t (id INT PRIMARY KEY)", 0);

    failures += expect_nonquery(first, "START TRANSACTION", 0);
    failures += expect_nonquery(first, "INSERT INTO t VALUES (1)", 1);
    failures += expect_nonquery(first, "SAVEPOINT same_name", 0);
    failures += expect_nonquery(first, "INSERT INTO t VALUES (2)", 1);

    failures += expect_nonquery(second, "START TRANSACTION", 0);
    failures += expect_nonquery(second, "INSERT INTO t VALUES (10)", 1);
    failures += expect_nonquery(second, "SAVEPOINT same_name", 0);
    failures += expect_nonquery(second, "INSERT INTO t VALUES (20)", 1);
    failures += expect_nonquery(second, "ROLLBACK TO same_name", 0);
    failures += expect_nonquery(second, "COMMIT", 0);

    failures += expect_nonquery(first, "ROLLBACK TO same_name", 0);
    failures += expect_nonquery(first, "COMMIT", 0);

    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id FROM t ORDER BY id",
            .values = first_handle_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first handle savepoint state",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id FROM t ORDER BY id",
            .values = second_handle_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second handle savepoint state",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);
    return failures;
}

static int test_independent_transaction_characteristic_handles(void) {
    static const char *const first_handle_rows[] = {"1"};
    static const char *const second_handle_rows[] = {"10", "20"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "transaction_characteristics_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "transaction_characteristics_second") !=
            0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += seed_schema(first);
    failures += seed_schema(second);
    failures += expect_nonquery(first, "CREATE TABLE t (id INT PRIMARY KEY)", 0);
    failures += expect_nonquery(second, "CREATE TABLE t (id INT PRIMARY KEY)", 0);
    failures += expect_nonquery(first, "INSERT INTO t VALUES (1)", 1);
    failures += expect_nonquery(second, "INSERT INTO t VALUES (10)", 1);

    failures += expect_nonquery(first, "SET SESSION TRANSACTION READ ONLY", 0);
    failures += expect_error_details(
        first,
        "INSERT INTO t VALUES (2)",
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    failures += expect_nonquery(second, "INSERT INTO t VALUES (20)", 1);

    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT id FROM t ORDER BY id",
            .values = first_handle_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first handle transaction characteristics",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT id FROM t ORDER BY id",
            .values = second_handle_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "second handle transaction characteristics",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);
    return failures;
}

static int test_read_only_transaction_does_not_reserve_writer_lock(void) {
    static const char *const rows[] = {"1", "10", "2", "20"};
    char path[test_path_capacity];
    mylite_db *reader = NULL;
    mylite_db *writer = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "read_only_writer_concurrency") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &reader), MYLITE_OK, "open read-only reader");
    failures += seed_schema(reader);
    failures += expect_nonquery(reader, "CREATE TABLE t (id INT PRIMARY KEY, v INT)", 0);
    failures += expect_nonquery(reader, "INSERT INTO t VALUES (1, 10)", 1);
    failures += expect_int(mylite_open(path, &writer), MYLITE_OK, "open concurrent writer");
    failures += expect_nonquery(writer, "USE app", 0);

    failures += expect_nonquery(reader, "START TRANSACTION READ ONLY", 0);
    failures += expect_nonquery(writer, "INSERT INTO t VALUES (2, 20)", 1);
    failures += expect_nonquery(reader, "COMMIT", 0);
    failures += expect_query_values(
        reader,
        (struct expected_query){
            .sql = "SELECT id, v FROM t ORDER BY id",
            .values = rows,
            .column_count = 2U,
            .row_count = 2U,
            .context = "concurrent writer after read-only transaction",
        }
    );

    mylite_close(writer);
    mylite_close(reader);
    remove_related_files(path);
    return failures;
}

#ifndef _WIN32
static int test_writer_waits_for_active_read_cursor(void) {
    static const char query[] = "SELECT id FROM t ORDER BY id";
    char path[test_path_capacity];
    mylite_db *reader = NULL;
    mylite_db *writer = NULL;
    mylite_stmt *cursor = NULL;
    pthread_t writer_thread;
    struct concurrent_write_context write = {0};
    struct writer_busy_wait_context busy_wait = {0};
    bool synchronization_initialized = false;
    int thread_was_created = 0;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "writer_busy_wait") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &reader), MYLITE_OK, "open busy-wait reader");
    failures += seed_schema(reader);
    failures += expect_nonquery(reader, "CREATE TABLE t (id INT PRIMARY KEY, v INT)", 0);
    failures += expect_nonquery(reader, "INSERT INTO t VALUES (1, 10), (2, 20)", 2);
    failures += expect_int(mylite_open(path, &writer), MYLITE_OK, "open busy-wait writer");
    failures += expect_nonquery(writer, "USE app", 0);
    failures += expect_int(
        mylite_prepare(reader, query, strlen(query), &cursor),
        MYLITE_OK,
        "prepare active read cursor"
    );
    failures += expect_int(mylite_stmt_step(cursor), MYLITE_ROW, "step active read cursor");

    write.database = writer;
    if (failures == 0 && pthread_mutex_init(&busy_wait.mutex, NULL) == 0) {
        if (pthread_cond_init(&busy_wait.condition, NULL) == 0) {
            synchronization_initialized = true;
            sqlite3_busy_handler(
                mylite_connection_sqlite_for_test(writer),
                wait_for_reader_release,
                &busy_wait
            );
        } else {
            (void)pthread_mutex_destroy(&busy_wait.mutex);
            fprintf(stderr, "initialize busy-wait condition: failed\n");
            failures++;
        }
    } else if (failures == 0) {
        fprintf(stderr, "initialize busy-wait mutex: failed\n");
        failures++;
    }
    if (failures == 0 &&
        pthread_create(&writer_thread, NULL, execute_concurrent_write, &write) == 0) {
        thread_was_created = 1;
        (void)pthread_mutex_lock(&busy_wait.mutex);
        while (!busy_wait.writer_blocked) {
            (void)pthread_cond_wait(&busy_wait.condition, &busy_wait.mutex);
        }
        (void)pthread_mutex_unlock(&busy_wait.mutex);
    } else if (failures == 0) {
        fprintf(stderr, "create busy-wait writer thread: failed\n");
        failures++;
    }

    failures += expect_int(mylite_stmt_finalize(cursor), MYLITE_OK, "release active read cursor");
    cursor = NULL;
    if (synchronization_initialized) {
        (void)pthread_mutex_lock(&busy_wait.mutex);
        busy_wait.reader_released = true;
        (void)pthread_cond_signal(&busy_wait.condition);
        (void)pthread_mutex_unlock(&busy_wait.mutex);
    }
    if (thread_was_created) {
        failures += expect_int(pthread_join(writer_thread, NULL), 0, "join busy-wait writer");
        failures += expect_int(write.rc, MYLITE_OK, "concurrent write waits for reader");
        failures += expect_int64(write.affected_rows, 1, "concurrent write affected rows");
    }
    if (synchronization_initialized) {
        sqlite3_busy_handler(mylite_connection_sqlite_for_test(writer), NULL, NULL);
        (void)pthread_cond_destroy(&busy_wait.condition);
        (void)pthread_mutex_destroy(&busy_wait.mutex);
    }

    mylite_close(writer);
    mylite_close(reader);
    remove_related_files(path);
    return failures;
}

static void *execute_concurrent_write(void *argument) {
    struct concurrent_write_context *write = argument;
    mylite_result *result = NULL;
    static const char sql[] = "UPDATE t SET v = 30 WHERE id = 1";

    write->rc = mylite_execute(write->database, sql, strlen(sql), &result);
    if (write->rc == MYLITE_OK && result != NULL) {
        write->affected_rows = mylite_result_affected_rows(result);
    }
    mylite_result_free(result);

    return NULL;
}

static int wait_for_reader_release(void *argument, int previous_attempts) {
    struct writer_busy_wait_context *busy_wait = argument;

    (void)previous_attempts;
    (void)pthread_mutex_lock(&busy_wait->mutex);
    busy_wait->writer_blocked = true;
    (void)pthread_cond_signal(&busy_wait->condition);
    while (!busy_wait->reader_released) {
        (void)pthread_cond_wait(&busy_wait->condition, &busy_wait->mutex);
    }
    (void)pthread_mutex_unlock(&busy_wait->mutex);
    return 1;
}
#endif

static int test_drop_table_missing_implicitly_commits_transaction(void) {
    static const char *const first_insert_committed[] = {"1"};
    static const char *const second_insert_committed[] = {"1"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "drop_missing_commit") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open drop-missing file");
    failures += seed_schema(database);
    failures += expect_nonquery(database, "CREATE TABLE t (id INT PRIMARY KEY, v INT)", 0);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (40, 400)", 1);
    failures += expect_error(database, "DROP TABLE missing_drop", mysql_error_unknown_table);
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t WHERE id = 40",
            .values = first_insert_committed,
            .column_count = 1U,
            .row_count = 1U,
            .context = "missing DROP TABLE implicitly commits before error",
        }
    );

    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "INSERT INTO t VALUES (50, 500)", 1);
    failures += expect_nonquery_with_warnings(
        database,
        "DROP TABLE IF EXISTS missing_if_exists",
        (struct expected_nonquery){0, 1U}
    );
    failures += expect_nonquery(database, "ROLLBACK", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM t WHERE id = 50",
            .values = second_insert_committed,
            .column_count = 1U,
            .row_count = 1U,
            .context = "missing DROP TABLE IF EXISTS implicitly commits before warning",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_file_close_rolls_back_transaction(void) {
    static const char *const committed_rows[] = {"1", "10"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "close_rollback") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file");
    failures += seed_schema(database);
    failures += expect_nonquery(database, "CREATE TABLE persisted (id INT PRIMARY KEY, v INT)", 0);
    failures += expect_nonquery(database, "INSERT INTO persisted VALUES (1, 10)", 1);
    failures += expect_nonquery(database, "START TRANSACTION", 0);
    failures += expect_nonquery(database, "SAVEPOINT close_sp", 0);
    failures += expect_nonquery(database, "INSERT INTO persisted VALUES (2, 20)", 1);
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen file");
    failures += expect_nonquery(database, "USE app", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM persisted ORDER BY id",
            .values = committed_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "close rolls back active transaction",
        }
    );

    failures += expect_nonquery(database, "BEGIN IMMEDIATE", 0);
    failures += expect_nonquery(database, "INSERT INTO persisted VALUES (3, 30)", 1);
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen begin immediate file");
    failures += expect_nonquery(database, "USE app", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v FROM persisted ORDER BY id",
            .values = committed_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "close rolls back active BEGIN IMMEDIATE transaction",
        }
    );
    mylite_close(database);

    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(actual_preamble),
        "transaction lifecycle preserves MyLite preamble"
    );

    remove_related_files(path);
    return failures;
}

static int seed_schema(mylite_db *database) {
    int failures = 0;

    failures += expect_nonquery(database, "CREATE DATABASE app", 1);
    failures += expect_nonquery(database, "USE app", 0);
    return failures;
}

static int expect_nonquery(mylite_db *database, const char *sql, int64_t affected_rows) {
    return expect_nonquery_with_warnings(
        database,
        sql,
        (struct expected_nonquery){affected_rows, 0U}
    );
}

static int expect_transaction_control(
    mylite_db *database,
    struct expected_transaction_control expected
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute_transaction_control(database, expected.statement, &result);

    failures += expect_int(rc, MYLITE_OK, expected.context);
    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_column_count(result), 0U, expected.context);
        failures += expect_size(mylite_result_row_count(result), 0U, expected.context);
        failures += expect_int64(
            mylite_result_affected_rows(result),
            expected.affected_rows,
            expected.context
        );
        failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    } else {
        fprintf(stderr, "%s failed: %s\n", expected.context, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_nonquery_with_warnings(
    mylite_db *database,
    const char *sql,
    struct expected_nonquery expected
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_OK, sql);
    if (rc == MYLITE_OK) {
        failures += expect_size(mylite_result_column_count(result), 0U, "nonquery column count");
        failures += expect_size(mylite_result_row_count(result), 0U, "nonquery row count");
        failures += expect_int64(mylite_result_affected_rows(result), expected.affected_rows, sql);
        failures += expect_size(
            mylite_result_warning_count(result),
            expected.warning_count,
            "nonquery warning count"
        );
    } else {
        fprintf(stderr, "%s failed: %s\n", sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_error(mylite_db *database, const char *sql, int expected_code) {
    return expect_error_details(database, sql, expected_code, NULL, NULL);
}

static int expect_error_details(
    mylite_db *database,
    const char *sql,
    int expected_code,
    const char *expected_sqlstate,
    const char *expected_message
) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    failures += expect_int(rc, MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected_code, "diagnostic code");
    if (expected_sqlstate != NULL) {
        failures +=
            expect_text(mylite_sqlstate(database), expected_sqlstate, "diagnostic SQLSTATE");
    }
    if (expected_message != NULL) {
        failures += expect_text(mylite_errmsg(database), expected_message, "diagnostic message");
    }
    failures += expect_size((size_t)(result != NULL), 0U, "error result");
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, query.sql, strlen(query.sql), &result);

    failures += expect_int(rc, MYLITE_OK, query.context);
    if (rc == MYLITE_OK) {
        failures +=
            expect_size(mylite_result_column_count(result), query.column_count, query.context);
        failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
        for (size_t row = 0U; row < query.row_count; ++row) {
            for (size_t column = 0U; column < query.column_count; ++column) {
                const size_t value_index = (row * query.column_count) + column;

                failures += expect_result_value(
                    result,
                    row,
                    column,
                    query.values == NULL ? NULL : query.values[value_index],
                    query.context
                );
            }
        }
    } else {
        fprintf(stderr, "%s failed: %s\n", query.sql, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return failures;
}

static int expect_row_count_zero(mylite_db *database, const char *context) {
    static const char *const values[] = {"0"};

    return expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .values = values,
            .column_count = 1U,
            .row_count = 1U,
            .context = context,
        }
    );
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    if (expected == NULL) {
        return expect_size((size_t)(actual != NULL), 0U, context);
    }
    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_transaction_lifecycle_%d_%s.mylite",
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path truncated\n");
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
    remove_with_suffix(path, "-shm");
    remove_with_suffix(path, "-wal");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related)) {
        return;
    }
    (void)remove(related);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    bytes_read = fread(buffer, 1U, size, file);
    fclose(file);
    return bytes_read == size ? 0 : 1;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    fprintf(stderr, "%s: expected \"%s\", got \"%s\"\n", context, expected, actual);
    return 1;
}

static int expect_contains(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && expected != NULL && strstr(actual, expected) != NULL) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected \"%s\" in \"%s\"\n",
        context,
        expected == NULL ? "(null)" : expected,
        actual == NULL ? "(null)" : actual
    );
    return 1;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) == 0) {
        return 0;
    }

    fprintf(stderr, "%s: bytes differ\n", context);
    return 1;
}
