#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "sqlite3.h"
#include "storage/mylite_file_open.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#  include <sys/wait.h>
#  include <unistd.h>
#else
#  include <process.h>
#endif

enum {
    model_row_capacity = 64,
    model_operation_count = 512,
    model_transaction_operation_count = 64,
    model_checkpoint_interval = 32,
    model_reopen_interval = 128,
    crash_round_count = 12,
    crash_value_scale = 10,
    integer_text_capacity = 32,
    path_capacity = 1024,
    path_slot_count = 8,
    path_suffix_capacity = 16,
    sql_capacity = 512,
};

struct model_row {
    bool present;
    int value;
};

struct schema_model {
    const char *table_name;
    const char *columns[3];
    size_t column_count;
    const char *secondary_index_name;
};

static char registered_paths[path_slot_count][path_capacity];
static size_t registered_path_count;
static unsigned int path_counter;
static bool cleanup_registered;

static int test_dml_model_and_reopen(void);
static int test_ddl_model_and_reopen(void);
#ifndef _WIN32
static int test_crash_commit_visibility(void);
static void run_crash_child(const char *path, int round, bool commit);
static int wait_for_crash_child(pid_t child);
#endif
static int test_ddl_fault_atomicity(void);
static uint32_t next_random(uint32_t *state);
static int apply_model_operation(
    mylite_db *database,
    struct model_row rows[model_row_capacity],
    uint32_t *random_state
);
static int assert_row_model(
    mylite_db *database,
    const struct model_row rows[model_row_capacity],
    const char *context
);
static int assert_schema_model(mylite_db *database, const struct schema_model *model);
static int run_ddl_fault_matrix(
    enum mylite_storage_vfs_fault_operation operation,
    const char *operation_name
);
static int prepare_path(char path[path_capacity]);
static int register_path(const char *path);
static void cleanup_paths(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int process_id(void);
static int reopen_database(const char *path, mylite_db **database);
static int open_model_database(const char *path, mylite_db **database);
static int execute_ok(mylite_db *database, const char *sql);
static int execute_transaction(
    mylite_db *database,
    enum mylite_transaction_control_statement statement
);
static int query_single_int(sqlite3 *sqlite, const char *sql, int *out_value);
static int query_single_text(sqlite3 *sqlite, const char *sql, const char *expected);
static int expect_true(bool condition, const char *context);

int main(void) {
    int failures = 0;
    int phase_failures = 0;

    phase_failures = test_dml_model_and_reopen();
    if (phase_failures != 0) {
        fprintf(stderr, "DML model phase failed: %d\n", phase_failures);
    }
    failures += phase_failures;
    phase_failures = test_ddl_model_and_reopen();
    if (phase_failures != 0) {
        fprintf(stderr, "DDL model phase failed: %d\n", phase_failures);
    }
    failures += phase_failures;
#ifndef _WIN32
    phase_failures = test_crash_commit_visibility();
    if (phase_failures != 0) {
        fprintf(stderr, "crash model phase failed: %d\n", phase_failures);
    }
    failures += phase_failures;
#endif
    phase_failures = test_ddl_fault_atomicity();
    if (phase_failures != 0) {
        fprintf(stderr, "DDL fault phase failed: %d\n", phase_failures);
    }
    failures += phase_failures;
    return failures == 0 ? 0 : 1;
}

static int test_dml_model_and_reopen(void) {
    struct model_row rows[model_row_capacity] = {{0}};
    struct model_row transaction_rows[model_row_capacity];
    char path[path_capacity];
    mylite_db *database = NULL;
    uint32_t random_state = UINT32_C(0x4d594c54);
    int failures = 0;

    failures += prepare_path(path);
    if (failures != 0) {
        return failures;
    }
    failures +=
        mylite_test_expect_int(open_model_database(path, &database), MYLITE_OK, "open DML model");
    failures += execute_ok(
        database,
        "CREATE TABLE model_rows(id INT NOT NULL PRIMARY KEY, value INT NOT NULL)"
    );

    for (size_t operation = 0U; operation < model_operation_count; ++operation) {
        failures += apply_model_operation(database, rows, &random_state);
        if ((operation + 1U) % model_checkpoint_interval == 0U) {
            failures += assert_row_model(database, rows, "DML model checkpoint");
        }
        if ((operation + 1U) % model_reopen_interval == 0U) {
            failures += reopen_database(path, &database);
            failures += assert_row_model(database, rows, "DML model reopen");
        }
    }

    memcpy(transaction_rows, rows, sizeof(transaction_rows));
    failures += execute_transaction(database, MYLITE_TRANSACTION_CONTROL_START);
    for (size_t operation = 0U; operation < model_transaction_operation_count; ++operation) {
        failures += apply_model_operation(database, transaction_rows, &random_state);
    }
    failures += execute_transaction(database, MYLITE_TRANSACTION_CONTROL_ROLLBACK);
    failures += reopen_database(path, &database);
    failures += assert_row_model(database, rows, "rollback/reopen invariant");

    memcpy(transaction_rows, rows, sizeof(transaction_rows));
    failures += execute_transaction(database, MYLITE_TRANSACTION_CONTROL_START);
    for (size_t operation = 0U; operation < model_transaction_operation_count; ++operation) {
        failures += apply_model_operation(database, transaction_rows, &random_state);
    }
    failures += execute_transaction(database, MYLITE_TRANSACTION_CONTROL_COMMIT);
    memcpy(rows, transaction_rows, sizeof(rows));
    failures += reopen_database(path, &database);
    failures += assert_row_model(database, rows, "commit/reopen invariant");

    mylite_close(database);
    return failures;
}

static int test_ddl_model_and_reopen(void) {
    struct schema_model model = {
        .table_name = "ddl_model",
        .columns = {"id", NULL, NULL},
        .column_count = 1U,
        .secondary_index_name = NULL,
    };
    char path[path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += prepare_path(path);
    if (failures != 0) {
        return failures;
    }
    failures +=
        mylite_test_expect_int(open_model_database(path, &database), MYLITE_OK, "open DDL model");
    failures += execute_ok(database, "CREATE TABLE ddl_model(id INT NOT NULL PRIMARY KEY)");
    failures += assert_schema_model(database, &model);

    failures += execute_ok(database, "ALTER TABLE ddl_model ADD COLUMN value INT DEFAULT 7");
    model.columns[1] = "value";
    model.column_count = 2U;
    failures += assert_schema_model(database, &model);

    failures += execute_ok(database, "CREATE INDEX idx_value ON ddl_model(value)");
    model.secondary_index_name = "idx_value";
    failures += assert_schema_model(database, &model);

    failures += execute_ok(database, "ALTER TABLE ddl_model RENAME COLUMN value TO score");
    model.columns[1] = "score";
    failures += assert_schema_model(database, &model);

    failures += execute_ok(database, "ALTER TABLE ddl_model RENAME INDEX idx_value TO idx_score");
    model.secondary_index_name = "idx_score";
    failures += assert_schema_model(database, &model);

    failures += execute_ok(database, "ALTER TABLE ddl_model RENAME TO ddl_model_renamed");
    model.table_name = "ddl_model_renamed";
    failures += assert_schema_model(database, &model);

    failures += execute_ok(database, "ALTER TABLE ddl_model_renamed DROP INDEX idx_score");
    model.secondary_index_name = NULL;
    failures += assert_schema_model(database, &model);

    failures += execute_ok(database, "ALTER TABLE ddl_model_renamed DROP COLUMN score");
    model.columns[1] = NULL;
    model.column_count = 1U;
    failures += reopen_database(path, &database);
    failures += assert_schema_model(database, &model);

    failures += execute_ok(database, "DROP TABLE ddl_model_renamed");
    failures += reopen_database(path, &database);
    failures += mylite_test_expect_int(
        mylite_execute(
            database,
            "SHOW TABLES LIKE 'ddl_model_renamed'",
            sizeof("SHOW TABLES LIKE 'ddl_model_renamed'") - 1U,
            &result
        ),
        MYLITE_OK,
        "query dropped modeled table"
    );
    if (result != NULL) {
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "modeled table is absent");
    }
    mylite_result_free(result);
    mylite_close(database);
    return failures;
}

#ifndef _WIN32
static int test_crash_commit_visibility(void) {
    struct model_row rows[model_row_capacity] = {{0}};
    char path[path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += prepare_path(path);
    if (failures != 0) {
        return failures;
    }
    failures +=
        mylite_test_expect_int(open_model_database(path, &database), MYLITE_OK, "open crash model");
    failures += execute_ok(
        database,
        "CREATE TABLE model_rows(id INT NOT NULL PRIMARY KEY, value INT NOT NULL)"
    );
    mylite_close(database);
    database = NULL;

    for (int round = 1; round <= crash_round_count; ++round) {
        pid_t child = fork();
        bool commit = round % 2 == 0;

        if (child == 0) {
            run_crash_child(path, round, commit);
        }
        if (child < 0) {
            fprintf(stderr, "fork crash model failed\n");
            return failures + 1;
        }
        failures += wait_for_crash_child(child);
        if (commit) {
            rows[round] = (struct model_row){.present = true, .value = round * crash_value_scale};
        }
        failures += mylite_test_expect_int(
            open_model_database(path, &database),
            MYLITE_OK,
            "reopen crash model"
        );
        failures += assert_row_model(database, rows, "crash commit visibility");
        mylite_close(database);
        database = NULL;
    }
    return failures;
}

static void run_crash_child(const char *path, int round, bool commit) {
    char sql[sql_capacity];
    mylite_db *database = NULL;
    int rc = open_model_database(path, &database);

    if (rc == MYLITE_OK) {
        rc = execute_transaction(database, MYLITE_TRANSACTION_CONTROL_START);
    }
    if (rc == MYLITE_OK) {
        (void)snprintf(
            sql,
            sizeof(sql),
            "INSERT INTO model_rows VALUES (%d, %d)",
            round,
            round * crash_value_scale
        );
        rc = execute_ok(database, sql);
    }
    if (rc == MYLITE_OK && commit) {
        rc = execute_transaction(database, MYLITE_TRANSACTION_CONTROL_COMMIT);
    }
    _exit(rc == MYLITE_OK ? 0 : 1);
}

static int wait_for_crash_child(pid_t child) {
    int child_status = 0;
    int failures = 0;

    if (waitpid(child, &child_status, 0) != child) {
        fprintf(stderr, "wait for crash model failed\n");
        return 1;
    }
    failures += expect_true(WIFEXITED(child_status), "crash model child exited");
    if (WIFEXITED(child_status)) {
        failures +=
            mylite_test_expect_int(WEXITSTATUS(child_status), 0, "crash model child status");
    }
    return failures;
}
#endif

static int test_ddl_fault_atomicity(void) {
    int failures = 0;

    failures += run_ddl_fault_matrix(MYLITE_STORAGE_VFS_FAULT_WRITE, "write");
    failures += run_ddl_fault_matrix(MYLITE_STORAGE_VFS_FAULT_SYNC, "sync");
    return failures;
}

static uint32_t next_random(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

static int apply_model_operation(
    mylite_db *database,
    struct model_row rows[model_row_capacity],
    uint32_t *random_state
) {
    char sql[sql_capacity];
    uint32_t random = next_random(random_state);
    size_t id = (size_t)(random % model_row_capacity);
    int value = (int)(next_random(random_state) % UINT32_C(100000));
    int written = 0;

    switch (random % 3U) {
    case 0U:
        written = snprintf(
            sql,
            sizeof(sql),
            "INSERT INTO model_rows VALUES (%zu, %d) "
            "ON DUPLICATE KEY UPDATE value = VALUES(value)",
            id,
            value
        );
        rows[id] = (struct model_row){.present = true, .value = value};
        break;
    case 1U:
        written = snprintf(
            sql,
            sizeof(sql),
            "UPDATE model_rows SET value = %d WHERE id = %zu",
            value,
            id
        );
        if (rows[id].present) {
            rows[id].value = value;
        }
        break;
    default:
        written = snprintf(sql, sizeof(sql), "DELETE FROM model_rows WHERE id = %zu", id);
        rows[id] = (struct model_row){0};
        break;
    }
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        return 1;
    }
    return execute_ok(database, sql);
}

static int assert_row_model(
    mylite_db *database,
    const struct model_row rows[model_row_capacity],
    const char *context
) {
    mylite_result *result = NULL;
    size_t expected_row_count = 0U;
    size_t result_row = 0U;
    int failures = 0;

    for (size_t id = 0U; id < model_row_capacity; ++id) {
        expected_row_count += rows[id].present ? 1U : 0U;
    }
    failures += mylite_test_expect_int(
        mylite_execute(
            database,
            "SELECT id, value FROM model_rows ORDER BY id",
            sizeof("SELECT id, value FROM model_rows ORDER BY id") - 1U,
            &result
        ),
        MYLITE_OK,
        context
    );
    if (result == NULL) {
        return failures + 1;
    }
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), expected_row_count, context);
    for (size_t id = 0U; id < model_row_capacity && result_row < mylite_result_row_count(result);
         ++id) {
        char expected_id[integer_text_capacity];
        char expected_value[integer_text_capacity];

        if (!rows[id].present) {
            continue;
        }
        (void)snprintf(expected_id, sizeof(expected_id), "%zu", id);
        (void)snprintf(expected_value, sizeof(expected_value), "%d", rows[id].value);
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, result_row, 0U),
            expected_id,
            context
        );
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, result_row, 1U),
            expected_value,
            context
        );
        ++result_row;
    }
    mylite_result_free(result);
    return failures;
}

static int assert_schema_model(mylite_db *database, const struct schema_model *model) {
    char sql[sql_capacity];
    mylite_result *result = NULL;
    size_t secondary_index_count = 0U;
    int failures = 0;
    int written = snprintf(sql, sizeof(sql), "SHOW COLUMNS FROM `%s`", model->table_name);

    if (written < 0 || (size_t)written >= sizeof(sql)) {
        return 1;
    }
    failures += mylite_test_expect_int(
        mylite_execute(database, sql, (size_t)written, &result),
        MYLITE_OK,
        sql
    );
    if (result == NULL) {
        return failures + 1;
    }
    failures += mylite_test_expect_size(mylite_result_row_count(result), model->column_count, sql);
    for (size_t column = 0U;
         column < model->column_count && column < mylite_result_row_count(result);
         ++column) {
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, column, 0U),
            model->columns[column],
            sql
        );
    }
    mylite_result_free(result);
    result = NULL;

    written = snprintf(sql, sizeof(sql), "SHOW INDEX FROM `%s`", model->table_name);
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        return failures + 1;
    }
    failures += mylite_test_expect_int(
        mylite_execute(database, sql, (size_t)written, &result),
        MYLITE_OK,
        sql
    );
    if (result == NULL) {
        return failures + 1;
    }
    for (size_t row = 0U; row < mylite_result_row_count(result); ++row) {
        const char *index_name = mylite_result_value_text(result, row, 2U);

        if (index_name != NULL && strcmp(index_name, "PRIMARY") != 0) {
            failures += mylite_test_expect_text(index_name, model->secondary_index_name, sql);
            ++secondary_index_count;
        }
    }
    failures += mylite_test_expect_size(
        secondary_index_count,
        model->secondary_index_name == NULL ? 0U : 1U,
        sql
    );
    mylite_result_free(result);
    return failures;
}

static int run_ddl_fault_matrix(
    enum mylite_storage_vfs_fault_operation operation,
    const char *operation_name
) {
    char path[path_capacity];
    size_t operation_call_count = 0U;
    int failures = 0;

    failures += prepare_path(path);
    if (failures != 0) {
        return failures;
    }

    {
        mylite_db *database = NULL;
        mylite_result *result = NULL;

        failures +=
            mylite_test_expect_int(open_model_database(path, &database), MYLITE_OK, operation_name);
        mylite_storage_vfs_test_set_fault(operation, SIZE_MAX);
        failures += mylite_test_expect_int(
            mylite_execute(
                database,
                "CREATE TABLE fault_table(id INT NOT NULL PRIMARY KEY, value INT)",
                sizeof("CREATE TABLE fault_table(id INT NOT NULL PRIMARY KEY, value INT)") - 1U,
                &result
            ),
            MYLITE_OK,
            "measure non-faulted DDL"
        );
        mylite_result_free(result);
        failures += expect_true(
            !mylite_storage_vfs_test_fault_was_triggered(),
            "measurement DDL does not trigger fault"
        );
        operation_call_count = mylite_storage_vfs_test_matching_call_count();
        failures += expect_true(operation_call_count > 0U, "DDL reaches measured VFS operation");
        mylite_storage_vfs_test_clear_fault();
        mylite_close(database);
    }

    for (size_t fail_on_call = 1U; fail_on_call <= operation_call_count; ++fail_on_call) {
        mylite_db *database = NULL;
        mylite_result *result = NULL;
        sqlite3 *sqlite = NULL;
        bool fault_triggered = false;
        int catalog_table_count = -1;
        int physical_table_count = -1;

        remove_related_files(path);
        failures +=
            mylite_test_expect_int(open_model_database(path, &database), MYLITE_OK, operation_name);
        mylite_storage_vfs_test_set_fault(operation, fail_on_call);
        (void)mylite_execute(
            database,
            "CREATE TABLE fault_table(id INT NOT NULL PRIMARY KEY, value INT)",
            sizeof("CREATE TABLE fault_table(id INT NOT NULL PRIMARY KEY, value INT)") - 1U,
            &result
        );
        mylite_result_free(result);
        fault_triggered = mylite_storage_vfs_test_fault_was_triggered();
        mylite_storage_vfs_test_clear_fault();
        mylite_close(database);
        database = NULL;

        failures += mylite_test_expect_int(
            open_model_database(path, &database),
            MYLITE_OK,
            "reopen DDL fault file"
        );
        sqlite = mylite_connection_sqlite_for_test(database);
        if (sqlite == NULL) {
            failures += 1;
        } else {
            failures += query_single_int(
                sqlite,
                "SELECT count(*) FROM _mylite_catalog_tables WHERE name = 'fault_table'",
                &catalog_table_count
            );
            failures += query_single_int(
                sqlite,
                "SELECT count(*) FROM sqlite_schema WHERE type = 'table' "
                "AND name NOT GLOB '_mylite_catalog_*' AND name NOT GLOB 'sqlite_*'",
                &physical_table_count
            );
            failures += query_single_text(sqlite, "PRAGMA integrity_check", "ok");
        }
        failures += expect_true(
            catalog_table_count == 0 || catalog_table_count == 1,
            "faulted DDL catalog state is pre or post"
        );
        failures += mylite_test_expect_int(
            physical_table_count,
            catalog_table_count,
            "faulted DDL physical/catalog atomicity"
        );
        failures += expect_true(fault_triggered, "measured DDL fault is triggered");
        mylite_close(database);
    }

    remove_related_files(path);
    return failures;
}

static int prepare_path(char path[path_capacity]) {
    int written = snprintf(
        path,
        path_capacity,
        "./mylite_recovery_model_%d_%u.mylite",
        process_id(),
        path_counter
    );

    ++path_counter;
    if (written < 0 || (size_t)written >= path_capacity || register_path(path) != 0) {
        return 1;
    }
    remove_related_files(path);
    return 0;
}

static int register_path(const char *path) {
    int written = 0;

    if (!cleanup_registered) {
        if (atexit(cleanup_paths) != 0) {
            return 1;
        }
        cleanup_registered = true;
    }
    if (registered_path_count >= path_slot_count) {
        return 1;
    }
    written = snprintf(
        registered_paths[registered_path_count],
        sizeof(registered_paths[registered_path_count]),
        "%s",
        path
    );
    if (written < 0 || (size_t)written >= sizeof(registered_paths[registered_path_count])) {
        return 1;
    }
    ++registered_path_count;
    return 0;
}

static void cleanup_paths(void) {
    for (size_t index = 0U; index < registered_path_count; ++index) {
        remove_related_files(registered_paths[index]);
    }
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[path_capacity + path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(related_path)) {
        (void)remove(related_path);
    }
}

static int process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static int reopen_database(const char *path, mylite_db **database) {
    int failures = 0;

    if (*database != NULL) {
        mylite_close(*database);
        *database = NULL;
    }
    failures += mylite_test_expect_int(
        open_model_database(path, database),
        MYLITE_OK,
        "reopen modeled database"
    );
    failures += expect_true(*database != NULL, "reopen returns modeled database");
    return failures;
}

static int open_model_database(const char *path, mylite_db **database) {
    int rc = mylite_open(path, database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (execute_ok(*database, "CREATE DATABASE IF NOT EXISTS model") != 0 ||
        execute_ok(*database, "USE model") != 0) {
        mylite_close(*database);
        *database = NULL;
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int execute_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return rc == MYLITE_OK ? 0 : 1;
}

static int execute_transaction(
    mylite_db *database,
    enum mylite_transaction_control_statement statement
) {
    mylite_result *result = NULL;
    int rc = mylite_execute_transaction_control(database, statement, &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "transaction %d failed: %s\n", (int)statement, mylite_errmsg(database));
    }
    mylite_result_free(result);
    return rc;
}

static int query_single_int(sqlite3 *sqlite, const char *sql, int *out_value) {
    sqlite3_stmt *statement = NULL;
    int rc = sqlite3_prepare_v2(sqlite, sql, -1, &statement, NULL);

    if (rc == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW) {
        *out_value = sqlite3_column_int(statement, 0);
    } else {
        fprintf(stderr, "%s: SQLite query failed: %s\n", sql, sqlite3_errmsg(sqlite));
        rc = SQLITE_ERROR;
    }
    sqlite3_finalize(statement);
    return rc == SQLITE_OK ? 0 : 1;
}

static int query_single_text(sqlite3 *sqlite, const char *sql, const char *expected) {
    sqlite3_stmt *statement = NULL;
    int rc = sqlite3_prepare_v2(sqlite, sql, -1, &statement, NULL);
    int failures = 0;

    if (rc == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW) {
        const char *actual = (const char *)sqlite3_column_text(statement, 0);

        failures += mylite_test_expect_text(actual, expected, sql);
    } else {
        fprintf(stderr, "%s: SQLite query failed: %s\n", sql, sqlite3_errmsg(sqlite));
        failures += 1;
    }
    sqlite3_finalize(statement);
    return failures;
}

static int expect_true(bool condition, const char *context) {
    if (condition) {
        return 0;
    }
    fprintf(stderr, "%s: expected true\n", context);
    return 1;
}
