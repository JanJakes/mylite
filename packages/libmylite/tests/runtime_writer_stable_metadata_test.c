#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
typedef intptr_t child_process;
#else
#  include <sys/wait.h>
#  include <unistd.h>
typedef pid_t child_process;
#endif

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
    sql_capacity = 4096,
    sqlite_use_nul_terminated_string = -1,
    process_child_argument_count = 6,
    process_child_operation_argument = 5,
    process_exec_failure_status = 127,
    process_stress_round_count = 32,
    path_wait_attempt_count = 10000,
    path_wait_sleep_ms = 1,
};

struct writer_race {
    mylite_db *writer;
    const char *ddl;
    int writer_rc;
    bool writer_triggered;
    bool writer_lock_seen;
    bool metadata_seen_before_writer_lock;
};

struct process_writer_race {
    const char *ready_path;
    const char *done_path;
    int barrier_rc;
    bool writer_triggered;
    bool writer_lock_seen;
    bool metadata_seen_before_writer_lock;
};

static int test_writer_stable_dml_matrix(void);
static int test_writer_stable_process_stress(const char *executable_path);
static int process_stress_child_main(
    const char *database_path,
    const char *ready_path,
    const char *done_path,
    const char *operation
);
static int run_process_stress_round(
    mylite_db *database,
    const char *database_path,
    const char *executable_path,
    const char *ready_path,
    const char *done_path,
    int round
);
static int spawn_process_stress_child(
    const char *executable_path,
    const char *database_path,
    const char *ready_path,
    const char *done_path,
    const char *operation,
    child_process *out_child
);
static int wait_process_stress_child(child_process child);
static int execute_process_writer_before_first_lock(
    unsigned int trace_kind,
    void *context,
    void *statement,
    void *detail
);
static int setup_dml_matrix(mylite_db *database);
static int run_direct_dml_matrix(mylite_db *database, mylite_db *writer, const char *import_path);
static int run_prepared_dml_matrix(mylite_db *database, mylite_db *writer);
static int run_transaction_dml_matrix(mylite_db *database, mylite_db *writer);
static int verify_dml_matrix(mylite_db *database);
static int verify_reopened_catalog(mylite_db *database);
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
static int run_sql_race(
    mylite_db *database,
    mylite_db *writer,
    const char *ddl,
    const char *sql,
    int expected_rc,
    const char *context
);
static int run_native_prepared_race(
    mylite_db *database,
    mylite_stmt *statement,
    mylite_db *writer,
    const char *ddl,
    const char *context
);
// NOLINTEND(bugprone-easily-swappable-parameters)
static int install_writer_race(mylite_db *database, struct writer_race *race);
static int finish_writer_race(
    mylite_db *database,
    const struct writer_race *race,
    const char *context
);
static int execute_writer_before_first_lock(
    unsigned int trace_kind,
    void *context,
    void *statement,
    void *detail
);
static int execute_ok(mylite_db *database, const char *sql);
static int expect_scalar(mylite_db *database, const char *sql, const char *expected);
static int expect_sqlite_int(sqlite3 *sqlite, const char *sql, int expected, const char *context);
static int expect_sqlite_text(
    sqlite3 *sqlite,
    const char *sql,
    const char *expected,
    const char *context
);
static int write_text_file(const char *path, const char *contents);
static int wait_for_path(const char *path);
static int path_exists(const char *path);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(int argc, char **argv) {
    int failures = 0;

    if (argc == process_child_argument_count && strcmp(argv[1], "--writer-child") == 0) {
        return process_stress_child_main(
            argv[2],
            argv[3],
            argv[4],
            argv[process_child_operation_argument]
        );
    }
    failures += test_writer_stable_dml_matrix();
    failures += test_writer_stable_process_stress(argv[0]);
    return failures == 0 ? 0 : 1;
}

static int test_writer_stable_dml_matrix(void) {
    char database_path[test_path_capacity];
    char import_path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_db *writer = NULL;
    int failures = 0;

    if (mylite_test_make_path(database_path, sizeof(database_path), "writer-stable-metadata") !=
            0 ||
        mylite_test_make_path(import_path, sizeof(import_path), "writer-stable-import") != 0) {
        return 1;
    }
    remove_related_files(database_path);
    (void)remove(import_path);

    failures += write_text_file(import_path, "2147483648\n");
    failures += mylite_test_expect_int(
        mylite_open(database_path, &database),
        MYLITE_OK,
        "open writer-stable database"
    );
    failures += setup_dml_matrix(database);
    failures += mylite_test_expect_int(
        mylite_open(database_path, &writer),
        MYLITE_OK,
        "open interleaved schema writer"
    );
    failures += execute_ok(writer, "USE app");

    failures += run_direct_dml_matrix(database, writer, import_path);
    failures += run_prepared_dml_matrix(database, writer);
    failures += run_transaction_dml_matrix(database, writer);
    failures += verify_dml_matrix(database);

    mylite_close(writer);
    mylite_close(database);
    writer = NULL;
    database = NULL;

    failures += mylite_test_expect_int(
        mylite_open(database_path, &database),
        MYLITE_OK,
        "reopen writer-stable database"
    );
    failures += execute_ok(database, "USE app");
    failures += verify_dml_matrix(database);
    failures += verify_reopened_catalog(database);
    failures += execute_ok(database, "UPDATE explicit_race SET value = 2147483649 WHERE id = 1");
    failures +=
        expect_scalar(database, "SELECT value FROM explicit_race WHERE id = 1", "2147483649");

    mylite_close(database);
    remove_related_files(database_path);
    (void)remove(import_path);
    return failures;
}

static int test_writer_stable_process_stress(const char *executable_path) {
    char database_path[test_path_capacity];
    char ready_path[test_path_capacity];
    char done_path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;
    int written = 0;

    if (mylite_test_make_path(database_path, sizeof(database_path), "writer-stable-process") != 0) {
        return 1;
    }
    written = snprintf(ready_path, sizeof(ready_path), "%s-ready", database_path);
    failures += mylite_test_expect_true(
        written >= 0 && (size_t)written < sizeof(ready_path),
        "build process-stress ready path"
    );
    written = snprintf(done_path, sizeof(done_path), "%s-done", database_path);
    failures += mylite_test_expect_true(
        written >= 0 && (size_t)written < sizeof(done_path),
        "build process-stress done path"
    );
    remove_related_files(database_path);
    (void)remove(ready_path);
    (void)remove(done_path);

    failures += mylite_test_expect_int(
        mylite_open(database_path, &database),
        MYLITE_OK,
        "open writer-stable process database"
    );
    if (database != NULL) {
        failures += execute_ok(database, "CREATE DATABASE app");
        failures += execute_ok(database, "USE app");
        failures +=
            execute_ok(database, "CREATE TABLE process_race (id INT PRIMARY KEY, value INT)");
        failures += execute_ok(database, "INSERT INTO process_race VALUES (1, 0)");
    }

    for (int round = 0; round < process_stress_round_count && failures == 0; ++round) {
        failures += run_process_stress_round(
            database,
            database_path,
            executable_path,
            ready_path,
            done_path,
            round
        );
    }
    if (database != NULL && failures == 0) {
        failures += expect_scalar(database, "SELECT value FROM process_race", "32");
        failures += expect_scalar(
            database,
            "SELECT COUNT(*) FROM information_schema.STATISTICS "
            "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'process_race' "
            "AND INDEX_NAME = 'idx_process_race'",
            "0"
        );
    }
    mylite_close(database);
    database = NULL;

    failures += mylite_test_expect_int(
        mylite_open(database_path, &database),
        MYLITE_OK,
        "reopen writer-stable process database"
    );
    if (database != NULL) {
        sqlite3 *sqlite = mylite_connection_sqlite_for_test(database);

        failures += execute_ok(database, "USE app");
        failures += expect_scalar(database, "SELECT value FROM process_race", "32");
        failures += expect_sqlite_int(
            sqlite,
            "SELECT integrity_catalog_generation = catalog_generation "
            "AND integrity_sqlite_schema_version = "
            "(SELECT schema_version FROM pragma_schema_version) "
            "FROM _mylite_catalog_state WHERE singleton_id = 1",
            1,
            "process-stress integrity seal matches durable state"
        );
        failures += expect_sqlite_text(
            sqlite,
            "PRAGMA integrity_check",
            "ok",
            "process-stress SQLite integrity"
        );
    }

    mylite_close(database);
    (void)remove(ready_path);
    (void)remove(done_path);
    remove_related_files(database_path);
    return failures;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
static int process_stress_child_main(
    const char *database_path,
    const char *ready_path,
    const char *done_path,
    const char *operation
) {
    static const char add_index_sql[] =
        "ALTER TABLE process_race ADD INDEX idx_process_race (value)";
    static const char drop_index_sql[] = "ALTER TABLE process_race DROP INDEX idx_process_race";
    const char *ddl = NULL;
    mylite_db *database = NULL;
    int failures = 0;

    if (strcmp(operation, "add-index") == 0) {
        ddl = add_index_sql;
    } else if (strcmp(operation, "drop-index") == 0) {
        ddl = drop_index_sql;
    } else {
        fprintf(stderr, "unknown process-stress operation: %s\n", operation);
        return 1;
    }

    failures += wait_for_path(ready_path);
    if (failures == 0) {
        failures += mylite_test_expect_int(
            mylite_open(database_path, &database),
            MYLITE_OK,
            "open process-stress DDL child"
        );
    }
    if (database != NULL) {
        failures += execute_ok(database, "USE app");
        failures += execute_ok(database, ddl);
    }
    mylite_close(database);
    failures += write_text_file(done_path, "");
    return failures == 0 ? 0 : 1;
}

// NOLINTEND(bugprone-easily-swappable-parameters)

static int run_process_stress_round(
    mylite_db *database,
    const char *database_path,
    const char *executable_path,
    const char *ready_path,
    const char *done_path,
    int round
) {
    char update_sql[sql_capacity];
    const char *operation = round % 2 == 0 ? "add-index" : "drop-index";
    child_process child = (child_process)-1;
    struct process_writer_race race = {
        .ready_path = ready_path,
        .done_path = done_path,
    };
    sqlite3 *sqlite = mylite_connection_sqlite_for_test(database);
    mylite_result *result = NULL;
    bool trace_installed = false;
    int failures = 0;
    int rc = MYLITE_ERROR;
    int written = snprintf(
        update_sql,
        sizeof(update_sql),
        "UPDATE process_race SET value = %d WHERE id = 1",
        round + 1
    );

    failures += mylite_test_expect_true(
        written >= 0 && (size_t)written < sizeof(update_sql),
        "build process-stress update"
    );
    (void)remove(ready_path);
    (void)remove(done_path);
    if (failures == 0) {
        failures += spawn_process_stress_child(
            executable_path,
            database_path,
            ready_path,
            done_path,
            operation,
            &child
        );
    }
    if (failures == 0 && sqlite != NULL) {
        failures += mylite_test_expect_int(
            sqlite3_trace_v2(
                sqlite,
                SQLITE_TRACE_STMT,
                execute_process_writer_before_first_lock,
                &race
            ),
            SQLITE_OK,
            "install process-stress trace"
        );
        trace_installed = failures == 0;
    } else if (sqlite == NULL) {
        failures++;
    }
    if (failures == 0) {
        rc = mylite_execute(database, update_sql, strlen(update_sql), &result);
        if (rc != MYLITE_OK) {
            fprintf(
                stderr,
                "process-stress DML failed in round %d: rc=%d err=%d state=%s msg=%s\n",
                round,
                rc,
                mylite_errcode(database),
                mylite_sqlstate(database),
                mylite_errmsg(database)
            );
        }
        failures += mylite_test_expect_int(rc, MYLITE_OK, "process-stress DML");
    }
    mylite_result_free(result);
    if (trace_installed) {
        failures += mylite_test_expect_int(
            sqlite3_trace_v2(sqlite, 0U, NULL, NULL),
            SQLITE_OK,
            "remove process-stress trace"
        );
    }
    if (!race.writer_triggered && child != (child_process)-1) {
        failures += write_text_file(ready_path, "");
    }
    if (child != (child_process)-1) {
        failures += wait_process_stress_child(child);
    }
    failures += mylite_test_expect_true(race.writer_lock_seen, "process-stress writer lock");
    failures += mylite_test_expect_true(race.writer_triggered, "process-stress DDL trigger");
    failures += mylite_test_expect_int(race.barrier_rc, 0, "process-stress DDL barrier");
    failures += mylite_test_expect_true(
        !race.metadata_seen_before_writer_lock,
        "process-stress planning follows writer lock"
    );
    (void)remove(ready_path);
    (void)remove(done_path);
    return failures;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
static int spawn_process_stress_child(
    const char *executable_path,
    const char *database_path,
    const char *ready_path,
    const char *done_path,
    const char *operation,
    child_process *out_child
) {
#ifdef _WIN32
    *out_child = _spawnl(
        _P_NOWAIT,
        executable_path,
        executable_path,
        "--writer-child",
        database_path,
        ready_path,
        done_path,
        operation,
        NULL
    );
#else
    *out_child = fork();
    if (*out_child == 0) {
        (void)execl(
            executable_path,
            executable_path,
            "--writer-child",
            database_path,
            ready_path,
            done_path,
            operation,
            (char *)NULL
        );
        _exit(process_exec_failure_status);
    }
#endif
    if (*out_child == (child_process)-1) {
        fprintf(stderr, "spawn process-stress DDL child failed\n");
        return 1;
    }
    return 0;
}

// NOLINTEND(bugprone-easily-swappable-parameters)

static int wait_process_stress_child(child_process child) {
    int status = -1;
    int failures = 0;

#ifdef _WIN32
    failures += mylite_test_expect_true(
        _cwait(&status, child, 0) != -1,
        "wait for process-stress DDL child"
    );
    failures += mylite_test_expect_int(status, 0, "process-stress DDL child status");
#else
    failures += mylite_test_expect_true(
        waitpid(child, &status, 0) == child,
        "wait for process-stress DDL child"
    );
    failures += mylite_test_expect_true(WIFEXITED(status), "process-stress DDL child exited");
    if (WIFEXITED(status)) {
        failures +=
            mylite_test_expect_int(WEXITSTATUS(status), 0, "process-stress DDL child status");
    }
#endif
    return failures;
}

/* sqlite3_trace_v2 fixes this callback's opaque parameter order. */
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
static int execute_process_writer_before_first_lock(
    unsigned int trace_kind,
    void *context,
    void *statement,
    void *detail
) {
    struct process_writer_race *race = context;
    const char *sql = NULL;

    (void)detail;
    if (trace_kind != SQLITE_TRACE_STMT || race == NULL || statement == NULL) {
        return 0;
    }
    sql = sqlite3_sql((sqlite3_stmt *)statement);
    if (sql == NULL) {
        return 0;
    }
    if (!race->writer_lock_seen && strstr(sql, "_mylite_catalog_") != NULL) {
        race->metadata_seen_before_writer_lock = true;
    }
    if (race->writer_triggered || strcmp(sql, "BEGIN IMMEDIATE") != 0) {
        return 0;
    }

    race->writer_lock_seen = true;
    race->writer_triggered = true;
    race->barrier_rc = write_text_file(race->ready_path, "");
    if (race->barrier_rc == 0) {
        race->barrier_rc = wait_for_path(race->done_path);
    }
    return 0;
}

// NOLINTEND(bugprone-easily-swappable-parameters)

static int setup_dml_matrix(mylite_db *database) {
    static const char *const statements[] = {
        "CREATE DATABASE app",
        "USE app",
        "CREATE TABLE insert_values_race (value INT)",
        "CREATE TABLE replace_values_race (value INT)",
        "CREATE TABLE insert_set_race (value INT)",
        "CREATE TABLE replace_set_race (value INT)",
        "CREATE TABLE select_source (value BIGINT)",
        "INSERT INTO select_source VALUES (2147483648)",
        "CREATE TABLE insert_select_race (value INT)",
        "CREATE TABLE replace_select_race (value INT)",
        "CREATE TABLE update_race (id INT PRIMARY KEY, value INT)",
        "INSERT INTO update_race VALUES (1, 1)",
        "CREATE TABLE delete_race (id INT PRIMARY KEY, value INT)",
        "INSERT INTO delete_race VALUES (1, 1)",
        "CREATE TABLE joined_update_race (id INT PRIMARY KEY, value INT)",
        "INSERT INTO joined_update_race VALUES (1, 1)",
        "CREATE TABLE joined_update_source (id INT PRIMARY KEY, value BIGINT)",
        "INSERT INTO joined_update_source VALUES (1, 2147483648)",
        "CREATE TABLE joined_delete_race (id INT PRIMARY KEY, value INT)",
        "INSERT INTO joined_delete_race VALUES (1, 1)",
        "CREATE TABLE joined_delete_source (id INT PRIMARY KEY)",
        "INSERT INTO joined_delete_source VALUES (1)",
        "CREATE TABLE duplicate_race (id INT PRIMARY KEY, value INT, UNIQUE KEY uq_value (value))",
        "INSERT INTO duplicate_race VALUES (1, 1)",
        "CREATE TABLE default_race (id INT PRIMARY KEY, value INT DEFAULT 1)",
        "CREATE TABLE nullability_race (id INT PRIMARY KEY, value INT NULL)",
        ("CREATE TABLE generated_race ("
         "id INT PRIMARY KEY, base INT, doubled BIGINT AS (base * 2) STORED)"),
        "CREATE TABLE fk_parent (id INT PRIMARY KEY)",
        "CREATE TABLE fk_child_race (id INT PRIMARY KEY, parent_id INT)",
        "CREATE TABLE load_race (value INT)",
        "CREATE TABLE native_prepared_race (id INT PRIMARY KEY, value INT)",
        "INSERT INTO native_prepared_race VALUES (1, 1)",
        "CREATE TABLE sql_prepared_race (id INT PRIMARY KEY, value INT)",
        "INSERT INTO sql_prepared_race VALUES (1, 1)",
        "CREATE TABLE autocommit_race (id INT PRIMARY KEY, value INT)",
        "INSERT INTO autocommit_race VALUES (1, 1)",
        "CREATE TABLE explicit_race (id INT PRIMARY KEY, value INT)",
        "INSERT INTO explicit_race VALUES (1, 1)",
    };
    int failures = 0;

    for (size_t index = 0U; index < sizeof(statements) / sizeof(statements[0U]); ++index) {
        failures += execute_ok(database, statements[index]);
    }
    return failures;
}

static int run_direct_dml_matrix(mylite_db *database, mylite_db *writer, const char *import_path) {
    char escaped_path[test_path_capacity * 2U];
    char load_sql[sql_capacity];
    int written = 0;
    int failures = 0;

    failures += run_sql_race(
        database,
        writer,
        "ALTER TABLE insert_values_race MODIFY COLUMN value BIGINT",
        "INSERT INTO insert_values_race VALUES (2147483648)",
        MYLITE_OK,
        "INSERT VALUES writer-stable planning"
    );
    failures += run_sql_race(
        database,
        writer,
        "ALTER TABLE replace_values_race MODIFY COLUMN value BIGINT",
        "REPLACE INTO replace_values_race VALUES (2147483648)",
        MYLITE_OK,
        "REPLACE VALUES writer-stable planning"
    );
    failures += run_sql_race(
        database,
        writer,
        "ALTER TABLE insert_set_race MODIFY COLUMN value BIGINT",
        "INSERT INTO insert_set_race SET value = 2147483648",
        MYLITE_OK,
        "INSERT SET writer-stable planning"
    );
    failures += run_sql_race(
        database,
        writer,
        "ALTER TABLE replace_set_race MODIFY COLUMN value BIGINT",
        "REPLACE INTO replace_set_race SET value = 2147483648",
        MYLITE_OK,
        "REPLACE SET writer-stable planning"
    );
    failures += run_sql_race(
        database,
        writer,
        "ALTER TABLE insert_select_race MODIFY COLUMN value BIGINT",
        "INSERT INTO insert_select_race SELECT value FROM select_source",
        MYLITE_OK,
        "INSERT SELECT writer-stable planning"
    );
    failures += run_sql_race(
        database,
        writer,
        "ALTER TABLE replace_select_race MODIFY COLUMN value BIGINT",
        "REPLACE INTO replace_select_race SELECT value FROM select_source",
        MYLITE_OK,
        "REPLACE SELECT writer-stable planning"
    );
    failures += run_sql_race(
        database,
        writer,
        "ALTER TABLE update_race MODIFY COLUMN value BIGINT",
        "UPDATE update_race SET value = 2147483648 WHERE id = 1",
        MYLITE_OK,
        "UPDATE writer-stable planning"
    );
    failures += run_sql_race(
        database,
        writer,
        "ALTER TABLE delete_race ADD INDEX idx_value (value)",
        "DELETE FROM delete_race WHERE value = 1",
        MYLITE_OK,
        "DELETE writer-stable planning"
    );
    failures += run_sql_race(
        database,
        writer,
        "ALTER TABLE joined_update_race MODIFY COLUMN value BIGINT",
        "UPDATE joined_update_race AS target "
        "JOIN joined_update_source AS source ON target.id = source.id "
        "SET target.value = 2147483648",
        MYLITE_OK,
        "joined UPDATE writer-stable planning"
    );
    failures += run_sql_race(
        database,
        writer,
        "ALTER TABLE joined_delete_race ADD COLUMN preserved INT DEFAULT 9",
        "DELETE target FROM joined_delete_race AS target "
        "JOIN joined_delete_source AS source ON target.id = source.id",
        MYLITE_OK,
        "joined DELETE writer-stable planning"
    );
    failures += run_sql_race(
        database,
        writer,
        "ALTER TABLE duplicate_race MODIFY COLUMN value BIGINT",
        "INSERT INTO duplicate_race VALUES (2, 2147483648) "
        "ON DUPLICATE KEY UPDATE value = VALUES(value)",
        MYLITE_OK,
        "duplicate-key writer-stable planning"
    );
    failures += run_sql_race(
        database,
        writer,
        "ALTER TABLE default_race ALTER COLUMN value SET DEFAULT 9",
        "INSERT INTO default_race (id) VALUES (1)",
        MYLITE_OK,
        "default writer-stable planning"
    );
    failures += run_sql_race(
        database,
        writer,
        "ALTER TABLE nullability_race MODIFY COLUMN value INT NOT NULL",
        "INSERT INTO nullability_race VALUES (1, NULL)",
        MYLITE_ERROR,
        "nullability writer-stable planning"
    );
    failures += run_sql_race(
        database,
        writer,
        "ALTER TABLE generated_race ADD INDEX idx_base (base)",
        "INSERT INTO generated_race (id, base) VALUES (1, 2)",
        MYLITE_OK,
        "generated-column writer-stable planning"
    );
    failures += run_sql_race(
        database,
        writer,
        "ALTER TABLE fk_child_race ADD CONSTRAINT fk_child_parent "
        "FOREIGN KEY (parent_id) REFERENCES fk_parent (id)",
        "INSERT INTO fk_child_race VALUES (1, 99)",
        MYLITE_ERROR,
        "foreign-key writer-stable planning"
    );

    failures += mylite_test_expect_int(
        mylite_test_escape_sql_string(escaped_path, sizeof(escaped_path), import_path),
        0,
        "escape writer-stable import path"
    );
    written = snprintf(
        load_sql,
        sizeof(load_sql),
        "LOAD DATA INFILE '%s' INTO TABLE load_race",
        escaped_path
    );
    failures += mylite_test_expect_true(
        written >= 0 && (size_t)written < sizeof(load_sql),
        "build writer-stable LOAD DATA statement"
    );
    if (written >= 0 && (size_t)written < sizeof(load_sql)) {
        failures += run_sql_race(
            database,
            writer,
            "ALTER TABLE load_race MODIFY COLUMN value BIGINT",
            load_sql,
            MYLITE_OK,
            "LOAD DATA writer-stable planning"
        );
    }

    return failures;
}

static int run_prepared_dml_matrix(mylite_db *database, mylite_db *writer) {
    mylite_stmt *statement = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_prepare(
            database,
            "UPDATE native_prepared_race SET value = ? WHERE id = 1",
            strlen("UPDATE native_prepared_race SET value = ? WHERE id = 1"),
            &statement
        ),
        MYLITE_OK,
        "prepare retained native DML"
    );
    if (statement != NULL) {
        failures += mylite_test_expect_int(
            mylite_stmt_bind_int64(statement, 0U, 2),
            MYLITE_OK,
            "bind first retained native DML value"
        );
        failures += mylite_test_expect_int(
            mylite_stmt_step(statement),
            MYLITE_DONE,
            "execute first retained native DML value"
        );
        failures += mylite_test_expect_int(
            mylite_stmt_reset(statement),
            MYLITE_OK,
            "reset retained native DML"
        );
        failures += mylite_test_expect_int(
            mylite_stmt_bind_int64(statement, 0U, INT64_C(2147483648)),
            MYLITE_OK,
            "bind replanned retained native DML value"
        );
        failures += run_native_prepared_race(
            database,
            statement,
            writer,
            "ALTER TABLE native_prepared_race MODIFY COLUMN value BIGINT",
            "reset native prepared DML writer-stable planning"
        );
    }
    failures += mylite_test_expect_int(
        mylite_stmt_finalize(statement),
        MYLITE_OK,
        "finalize retained native DML"
    );

    failures += execute_ok(
        database,
        "PREPARE sql_writer_race FROM "
        "'UPDATE sql_prepared_race SET value = ? WHERE id = 1'"
    );
    failures += execute_ok(database, "SET @writer_race_value = 2147483648");
    failures += run_sql_race(
        database,
        writer,
        "ALTER TABLE sql_prepared_race MODIFY COLUMN value BIGINT",
        "EXECUTE sql_writer_race USING @writer_race_value",
        MYLITE_OK,
        "SQL prepared DML writer-stable planning"
    );
    failures += execute_ok(database, "DEALLOCATE PREPARE sql_writer_race");
    return failures;
}

static int run_transaction_dml_matrix(mylite_db *database, mylite_db *writer) {
    int failures = 0;

    failures += execute_ok(database, "SET autocommit = 0");
    failures += run_sql_race(
        database,
        writer,
        "ALTER TABLE autocommit_race MODIFY COLUMN value BIGINT",
        "UPDATE autocommit_race SET value = 2147483648 WHERE id = 1",
        MYLITE_OK,
        "autocommit-disabled first write planning"
    );
    failures += execute_ok(database, "COMMIT");
    failures += execute_ok(database, "SET autocommit = 1");

    failures += run_sql_race(
        database,
        writer,
        "ALTER TABLE explicit_race MODIFY COLUMN value BIGINT",
        "START TRANSACTION",
        MYLITE_OK,
        "explicit transaction writer snapshot"
    );
    failures += execute_ok(database, "UPDATE explicit_race SET value = 2147483648 WHERE id = 1");
    failures += execute_ok(database, "COMMIT");
    return failures;
}

static int verify_dml_matrix(mylite_db *database) {
    static const struct {
        const char *sql;
        const char *expected;
    } expectations[] = {
        {"SELECT value FROM insert_values_race", "2147483648"},
        {"SELECT value FROM replace_values_race", "2147483648"},
        {"SELECT value FROM insert_set_race", "2147483648"},
        {"SELECT value FROM replace_set_race", "2147483648"},
        {"SELECT value FROM insert_select_race", "2147483648"},
        {"SELECT value FROM replace_select_race", "2147483648"},
        {"SELECT value FROM update_race WHERE id = 1", "2147483648"},
        {"SELECT COUNT(*) FROM delete_race", "0"},
        {"SELECT value FROM joined_update_race WHERE id = 1", "2147483648"},
        {"SELECT COUNT(*) FROM joined_delete_race", "0"},
        {"SELECT COUNT(*) FROM information_schema.COLUMNS "
         "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'joined_delete_race' "
         "AND COLUMN_NAME = 'preserved'",
         "1"},
        {"SELECT value FROM duplicate_race WHERE id = 2", "2147483648"},
        {"SELECT value FROM default_race WHERE id = 1", "9"},
        {"SELECT COUNT(*) FROM nullability_race", "0"},
        {"SELECT doubled FROM generated_race WHERE id = 1", "4"},
        {"SELECT COUNT(*) FROM fk_child_race", "0"},
        {"SELECT value FROM load_race", "2147483648"},
        {"SELECT value FROM native_prepared_race WHERE id = 1", "2147483648"},
        {"SELECT value FROM sql_prepared_race WHERE id = 1", "2147483648"},
        {"SELECT value FROM autocommit_race WHERE id = 1", "2147483648"},
        {"SELECT value FROM explicit_race WHERE id = 1", "2147483648"},
        {"SELECT COUNT(*) FROM information_schema.STATISTICS "
         "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'delete_race' "
         "AND INDEX_NAME = 'idx_value'",
         "1"},
        {"SELECT COUNT(*) FROM information_schema.REFERENTIAL_CONSTRAINTS "
         "WHERE CONSTRAINT_SCHEMA = 'app' AND TABLE_NAME = 'fk_child_race' "
         "AND CONSTRAINT_NAME = 'fk_child_parent'",
         "1"},
    };

    int failures = 0;

    for (size_t index = 0U; index < sizeof(expectations) / sizeof(expectations[0U]); ++index) {
        failures += expect_scalar(database, expectations[index].sql, expectations[index].expected);
    }
    return failures;
}

static int verify_reopened_catalog(mylite_db *database) {
    sqlite3 *sqlite = mylite_connection_sqlite_for_test(database);
    int failures = 0;

    if (sqlite == NULL) {
        fprintf(stderr, "reopened writer-stable database has no SQLite handle\n");
        return 1;
    }
    failures += expect_sqlite_int(
        sqlite,
        "SELECT COUNT(*) FROM _mylite_catalog_columns AS c "
        "JOIN _mylite_catalog_tables AS t ON t.table_id = c.table_id "
        "WHERE t.kind = 1 AND NOT EXISTS ("
        "SELECT 1 FROM pragma_table_xinfo(t.physical_name) AS p WHERE p.name = c.name)",
        0,
        "reopened catalog columns all have physical columns"
    );
    failures += expect_sqlite_int(
        sqlite,
        "SELECT COUNT(*) FROM _mylite_catalog_tables AS t "
        "JOIN pragma_table_xinfo(t.physical_name) AS p "
        "WHERE t.kind = 1 AND NOT EXISTS ("
        "SELECT 1 FROM _mylite_catalog_columns AS c "
        "WHERE c.table_id = t.table_id AND c.name = p.name)",
        0,
        "reopened physical columns all have catalog columns"
    );
    failures += expect_sqlite_int(
        sqlite,
        "SELECT COUNT(*) FROM _mylite_catalog_columns AS c "
        "JOIN _mylite_catalog_tables AS t ON t.table_id = c.table_id "
        "JOIN pragma_table_xinfo(t.physical_name) AS p ON p.name = c.name "
        "WHERE t.kind = 1 AND (UPPER(TRIM(p.type)) <> UPPER(TRIM(c.physical_type)) "
        "OR (c.is_nullable = 1 AND p.\"notnull\" <> 0) "
        "OR p.hidden <> CASE WHEN c.is_generated = 0 THEN 0 "
        "WHEN c.generated_kind = 2 THEN 3 ELSE 2 END)",
        0,
        "reopened catalog and physical column definitions match"
    );
    failures += expect_sqlite_int(
        sqlite,
        "SELECT COUNT(*) FROM _mylite_catalog_indexes AS i "
        "JOIN _mylite_catalog_tables AS t ON t.table_id = i.table_id "
        "LEFT JOIN sqlite_schema AS s ON s.type = 'index' "
        "AND s.name = i.physical_name AND s.tbl_name = t.physical_name "
        "WHERE i.kind IN (1, 2) AND s.name IS NULL",
        0,
        "reopened catalog indexes all have physical indexes"
    );
    failures += expect_sqlite_int(
        sqlite,
        "SELECT COUNT(*) FROM _mylite_catalog_tables AS t "
        "JOIN pragma_index_list(t.physical_name) AS p "
        "WHERE t.kind = 1 AND p.origin = 'c' AND NOT EXISTS ("
        "SELECT 1 FROM _mylite_catalog_indexes AS i "
        "WHERE i.table_id = t.table_id AND i.physical_name = p.name)",
        0,
        "reopened physical indexes all have catalog indexes"
    );
    failures += expect_sqlite_int(
        sqlite,
        "SELECT COUNT(*) FROM _mylite_catalog_foreign_keys AS fk "
        "JOIN _mylite_catalog_tables AS child ON child.table_id = fk.child_table_id "
        "JOIN _mylite_catalog_tables AS parent ON parent.table_id = fk.parent_table_id "
        "JOIN _mylite_catalog_foreign_key_columns AS fkc "
        "ON fkc.foreign_key_id = fk.foreign_key_id "
        "JOIN _mylite_catalog_columns AS child_column "
        "ON child_column.column_id = fkc.child_column_id "
        "JOIN _mylite_catalog_columns AS parent_column "
        "ON parent_column.column_id = fkc.parent_column_id "
        "WHERE child.name = 'fk_child_race' AND parent.name = 'fk_parent' "
        "AND fk.name = 'fk_child_parent' AND child_column.name = 'parent_id' "
        "AND parent_column.name = 'id'",
        1,
        "reopened foreign-key catalog definition matches"
    );
    failures += expect_sqlite_int(
        sqlite,
        "SELECT integrity_catalog_generation = catalog_generation "
        "AND integrity_sqlite_schema_version = "
        "(SELECT schema_version FROM pragma_schema_version) "
        "FROM _mylite_catalog_state WHERE singleton_id = 1",
        1,
        "reopened integrity seal matches durable state"
    );
    failures +=
        expect_sqlite_text(sqlite, "PRAGMA integrity_check", "ok", "reopened SQLite integrity");
    return failures;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
static int run_sql_race(
    mylite_db *database,
    mylite_db *writer,
    const char *ddl,
    const char *sql,
    int expected_rc,
    const char *context
) {
    struct writer_race race = {
        .writer = writer,
        .ddl = ddl,
        .writer_rc = MYLITE_ERROR,
    };
    mylite_result *result = NULL;
    int failures = install_writer_race(database, &race);
    int rc = MYLITE_ERROR;

    if (failures == 0) {
        rc = mylite_execute(database, sql, strlen(sql), &result);
        if (rc != expected_rc) {
            fprintf(
                stderr,
                "%s failed for %s: rc=%d err=%d state=%s msg=%s\n",
                context,
                sql,
                rc,
                mylite_errcode(database),
                mylite_sqlstate(database),
                mylite_errmsg(database)
            );
        }
        failures += mylite_test_expect_int(rc, expected_rc, context);
        failures += finish_writer_race(database, &race, context);
    }
    mylite_result_free(result);
    return failures;
}

static int run_native_prepared_race(
    mylite_db *database,
    mylite_stmt *statement,
    mylite_db *writer,
    const char *ddl,
    const char *context
) {
    struct writer_race race = {
        .writer = writer,
        .ddl = ddl,
        .writer_rc = MYLITE_ERROR,
    };
    int failures = install_writer_race(database, &race);

    if (failures == 0) {
        failures += mylite_test_expect_int(mylite_stmt_step(statement), MYLITE_DONE, context);
        failures += finish_writer_race(database, &race, context);
    }
    return failures;
}

// NOLINTEND(bugprone-easily-swappable-parameters)

static int install_writer_race(mylite_db *database, struct writer_race *race) {
    sqlite3 *sqlite = mylite_connection_sqlite_for_test(database);

    if (sqlite == NULL) {
        fprintf(stderr, "writer-race database has no SQLite handle\n");
        return 1;
    }
    return mylite_test_expect_int(
        sqlite3_trace_v2(sqlite, SQLITE_TRACE_STMT, execute_writer_before_first_lock, race),
        SQLITE_OK,
        "install writer-stable trace"
    );
}

static int finish_writer_race(
    mylite_db *database,
    const struct writer_race *race,
    const char *context
) {
    sqlite3 *sqlite = mylite_connection_sqlite_for_test(database);
    int failures = 0;

    if (sqlite == NULL) {
        return 1;
    }
    failures += mylite_test_expect_int(
        sqlite3_trace_v2(sqlite, 0U, NULL, NULL),
        SQLITE_OK,
        "remove writer-stable trace"
    );
    failures += mylite_test_expect_true(race->writer_lock_seen, context);
    failures += mylite_test_expect_true(race->writer_triggered, context);
    failures += mylite_test_expect_int(race->writer_rc, MYLITE_OK, context);
    failures += mylite_test_expect_true(!race->metadata_seen_before_writer_lock, context);
    return failures;
}

/* sqlite3_trace_v2 fixes this callback's opaque parameter order. */
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
static int execute_writer_before_first_lock(
    unsigned int trace_kind,
    void *context,
    void *statement,
    void *detail
) {
    struct writer_race *race = context;
    const char *sql = NULL;
    mylite_result *result = NULL;

    (void)detail;
    if (trace_kind != SQLITE_TRACE_STMT || race == NULL || statement == NULL) {
        return 0;
    }
    sql = sqlite3_sql((sqlite3_stmt *)statement);
    if (sql == NULL) {
        return 0;
    }
    if (!race->writer_lock_seen && strstr(sql, "_mylite_catalog_") != NULL) {
        race->metadata_seen_before_writer_lock = true;
    }
    if (race->writer_triggered || strcmp(sql, "BEGIN IMMEDIATE") != 0) {
        return 0;
    }

    race->writer_lock_seen = true;
    race->writer_triggered = true;
    race->writer_rc = mylite_execute(race->writer, race->ddl, strlen(race->ddl), &result);
    if (race->writer_rc != MYLITE_OK) {
        fprintf(
            stderr,
            "interleaved DDL failed for %s: rc=%d err=%d state=%s msg=%s\n",
            race->ddl,
            race->writer_rc,
            mylite_errcode(race->writer),
            mylite_sqlstate(race->writer),
            mylite_errmsg(race->writer)
        );
    }
    mylite_result_free(result);
    return 0;
}

// NOLINTEND(bugprone-easily-swappable-parameters)

static int execute_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "expected success for %s: rc=%d err=%d state=%s msg=%s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
    }
    mylite_result_free(result);
    return rc == MYLITE_OK ? 0 : 1;
}

static int expect_scalar(mylite_db *database, const char *sql, const char *expected) {
    mylite_result *result = NULL;
    const char *actual = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "expected scalar success for %s: rc=%d err=%d state=%s msg=%s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, sql);
    failures += mylite_test_expect_size(mylite_result_column_count(result), 1U, sql);
    actual = mylite_result_value_text(result, 0U, 0U);
    failures += mylite_test_expect_text(actual, expected, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_sqlite_int(sqlite3 *sqlite, const char *sql, int expected, const char *context) {
    sqlite3_stmt *statement = NULL;
    int actual = 0;
    int rc = sqlite3_prepare_v2(sqlite, sql, sqlite_use_nul_terminated_string, &statement, NULL);

    if (rc == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW) {
        actual = sqlite3_column_int(statement, 0);
    } else {
        fprintf(stderr, "%s: SQLite scalar query failed: %s\n", context, sqlite3_errmsg(sqlite));
        rc = SQLITE_ERROR;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK && rc == SQLITE_OK) {
        rc = SQLITE_ERROR;
    }
    return rc == SQLITE_OK ? mylite_test_expect_int(actual, expected, context) : 1;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
static int expect_sqlite_text(
    sqlite3 *sqlite,
    const char *sql,
    const char *expected,
    const char *context
) {
    sqlite3_stmt *statement = NULL;
    const unsigned char *actual = NULL;
    int failures = 0;
    int rc = sqlite3_prepare_v2(sqlite, sql, sqlite_use_nul_terminated_string, &statement, NULL);

    if (rc == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW) {
        actual = sqlite3_column_text(statement, 0);
        failures += mylite_test_expect_text((const char *)actual, expected, context);
    } else {
        fprintf(stderr, "%s: SQLite text query failed: %s\n", context, sqlite3_errmsg(sqlite));
        failures += 1;
    }
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        failures += 1;
    }
    return failures;
}

static int write_text_file(const char *path, const char *contents) {
    FILE *file = fopen(path, "wb");
    size_t size = strlen(contents);

    if (file == NULL) {
        fprintf(stderr, "failed to create %s\n", path);
        return 1;
    }
    if (fwrite(contents, 1U, size, file) != size) {
        fprintf(stderr, "failed to write %s\n", path);
        (void)fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s\n", path);
        return 1;
    }
    return 0;
}

// NOLINTEND(bugprone-easily-swappable-parameters)

static int wait_for_path(const char *path) {
    for (int attempt = 0; attempt < path_wait_attempt_count; ++attempt) {
        if (path_exists(path)) {
            return 0;
        }
        (void)sqlite3_sleep(path_wait_sleep_ms);
    }
    fprintf(stderr, "timed out waiting for barrier path: %s\n", path);
    return 1;
}

static int path_exists(const char *path) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        return 0;
    }
    (void)fclose(file);
    return 1;
}

static void remove_related_files(const char *path) {
    (void)remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(related)) {
        (void)remove(related);
    }
}
