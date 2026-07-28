#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "sqlite3.h"
#include "storage/mylite_file_open.h"

#include <errno.h>
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
    decimal_radix = 10,
    integer_text_capacity = 32,
    path_capacity = 1024,
    path_slot_count = 64,
    path_suffix_capacity = 16,
    sql_capacity = 512,
    ddl_fault_table_column_count = 2,
    ddl_fault_setup_row_count = 2,
    ddl_fault_setup_id_sum = 3,
    ddl_fault_setup_value_sum = 33,
    catalog_integrity_column_count = 2,
    catalog_previous_schema_version = MYLITE_CATALOG_SCHEMA_VERSION - 1,
    migration_death_child_argument_count = 5,
};

enum ddl_fault_state {
    ddl_fault_state_invalid = -1,
    ddl_fault_state_pre = 0,
    ddl_fault_state_post = 1,
};

enum ddl_fault_scenario_kind {
    ddl_fault_create_table = 0,
    ddl_fault_modify_column = 1,
    ddl_fault_drop_table = 2,
    ddl_fault_rename_table = 3,
    ddl_fault_create_index = 4,
    ddl_fault_drop_index = 5,
    ddl_fault_truncate_table = 6,
};

struct ddl_fault_scenario {
    enum ddl_fault_scenario_kind kind;
    const char *name;
    const char *sql;
};

struct fault_table_rows {
    int count;
    int id_sum;
    int value_sum;
};

struct ddl_fault_observation {
    int old_table_count;
    int renamed_table_count;
    int column_count;
    int bigint_column_count;
    int index_count;
    struct fault_table_rows rows;
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
    const struct ddl_fault_scenario *scenario,
    enum mylite_storage_vfs_fault_operation operation,
    bool use_truncate_journal
);
static int setup_ddl_fault_scenario(mylite_db *database, const struct ddl_fault_scenario *scenario);
static int classify_ddl_fault_state(
    mylite_db *database,
    const struct ddl_fault_scenario *scenario,
    enum ddl_fault_state *out_state
);
static enum ddl_fault_state classify_create_table(const struct ddl_fault_observation *observation);
static enum ddl_fault_state classify_modify_column(const struct ddl_fault_observation *state);
static enum ddl_fault_state classify_drop_table(const struct ddl_fault_observation *observation);
static enum ddl_fault_state classify_rename_table(const struct ddl_fault_observation *observation);
static enum ddl_fault_state classify_create_index(const struct ddl_fault_observation *observation);
static enum ddl_fault_state classify_drop_index(const struct ddl_fault_observation *observation);
static enum ddl_fault_state classify_truncate_table(const struct ddl_fault_observation *state);
static bool ddl_fault_has_original_int_table(const struct ddl_fault_observation *observation);
static bool ddl_fault_has_renamed_int_table(const struct ddl_fault_observation *observation);
static bool ddl_fault_has_setup_rows(const struct ddl_fault_observation *observation);
static bool ddl_fault_has_no_rows(const struct ddl_fault_observation *observation);
static int query_fault_table_rows(
    mylite_db *database,
    const char *table_name,
    struct fault_table_rows *out_rows
);
static int assert_recovery_invariants(const char *path, mylite_db *database);
static int query_catalog_generation(mylite_db *database, int *out_generation);
static int query_integrity_seal_matches(sqlite3 *sqlite, bool *out_matches);
static int configure_truncate_journal(mylite_db *database);
static int close_after_fault(mylite_db *database);
static int test_catalog_migration_fault_atomicity(void);
static int test_catalog_migration_process_death(const char *executable_path);
static int run_catalog_migration_process_death_matrix(
    const char *executable_path,
    enum mylite_storage_vfs_fault_operation operation,
    const char *operation_name
);
static int run_migration_process_death_child(
    const char *executable_path,
    const char *path,
    enum mylite_storage_vfs_fault_operation operation,
    const char *operation_name,
    size_t exit_on_call
);
static int migration_process_death_child_main(
    const char *path,
    enum mylite_storage_vfs_fault_operation operation,
    size_t exit_on_call
);
static int parse_migration_process_death_operation(
    const char *text,
    enum mylite_storage_vfs_fault_operation *out_operation
);
static int parse_positive_size(const char *text, size_t *out_value);
static int run_catalog_migration_fault_matrix(
    enum mylite_storage_vfs_fault_operation operation,
    const char *operation_name
);
static int setup_downgraded_catalog(const char *path, bool use_truncate_journal);
static int classify_catalog_migration_state(sqlite3 *sqlite, enum ddl_fault_state *out_state);
static int prepare_path(char path[path_capacity]);
static int register_path(const char *path);
static void cleanup_paths(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int file_exists_with_suffix(const char *path, const char *suffix);
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
static int query_single_text_value(
    sqlite3 *sqlite,
    const char *sql,
    char *destination,
    size_t destination_size
);
static int query_sqlite_ints(sqlite3 *sqlite, const char *sql, struct fault_table_rows *out_values);
static int expect_true(bool condition, const char *context);

int main(int argc, char **argv) {
    int failures = 0;
    int phase_failures = 0;

    if (argc == migration_death_child_argument_count &&
        strcmp(argv[1], "--migration-death-child") == 0) {
        enum mylite_storage_vfs_fault_operation operation = MYLITE_STORAGE_VFS_FAULT_NONE;
        size_t exit_on_call = 0U;

        if (parse_migration_process_death_operation(argv[3], &operation) != 0 ||
            parse_positive_size(argv[4], &exit_on_call) != 0) {
            return 1;
        }
        return migration_process_death_child_main(argv[2], operation, exit_on_call);
    }

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
    phase_failures = test_catalog_migration_process_death(argv[0]);
    if (phase_failures != 0) {
        fprintf(stderr, "catalog migration process-death phase failed: %d\n", phase_failures);
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
    static const struct ddl_fault_scenario scenarios[] = {
        {
            .kind = ddl_fault_create_table,
            .name = "create-table",
            .sql = "CREATE TABLE fault_table(id INT NOT NULL PRIMARY KEY, value INT NOT NULL)",
        },
        {
            .kind = ddl_fault_modify_column,
            .name = "modify-column-rebuild",
            .sql = "ALTER TABLE fault_table MODIFY value BIGINT NOT NULL",
        },
        {
            .kind = ddl_fault_drop_table,
            .name = "drop-table",
            .sql = "DROP TABLE fault_table",
        },
        {
            .kind = ddl_fault_rename_table,
            .name = "rename-table",
            .sql = "ALTER TABLE fault_table RENAME TO fault_table_renamed",
        },
        {
            .kind = ddl_fault_create_index,
            .name = "create-index",
            .sql = "CREATE INDEX fault_index ON fault_table(value)",
        },
        {
            .kind = ddl_fault_drop_index,
            .name = "drop-index",
            .sql = "DROP INDEX fault_index ON fault_table",
        },
        {
            .kind = ddl_fault_truncate_table,
            .name = "truncate-table",
            .sql = "TRUNCATE TABLE fault_table",
        },
    };
    const struct ddl_fault_scenario *modify_column_scenario = NULL;
    int failures = 0;

    for (size_t index = 0U; index < sizeof(scenarios) / sizeof(scenarios[0U]); ++index) {
        if (scenarios[index].kind == ddl_fault_modify_column) {
            modify_column_scenario = &scenarios[index];
        }
        failures += run_ddl_fault_matrix(&scenarios[index], MYLITE_STORAGE_VFS_FAULT_WRITE, false);
        failures += run_ddl_fault_matrix(&scenarios[index], MYLITE_STORAGE_VFS_FAULT_SYNC, false);
    }
    failures += expect_true(
        modify_column_scenario != NULL,
        "fault matrix includes a rebuilding DDL scenario"
    );
    if (modify_column_scenario != NULL) {
        failures +=
            run_ddl_fault_matrix(modify_column_scenario, MYLITE_STORAGE_VFS_FAULT_DELETE, false);
        failures +=
            run_ddl_fault_matrix(modify_column_scenario, MYLITE_STORAGE_VFS_FAULT_CLOSE, false);
        failures +=
            run_ddl_fault_matrix(modify_column_scenario, MYLITE_STORAGE_VFS_FAULT_TRUNCATE, true);
    }
    failures += test_catalog_migration_fault_atomicity();
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
    const struct ddl_fault_scenario *scenario,
    enum mylite_storage_vfs_fault_operation operation,
    bool use_truncate_journal
) {
    char path[path_capacity];
    size_t operation_call_count = 0U;
    int expected_post_generation = 0;
    int expected_pre_generation = 0;
    int failures = 0;

    failures += prepare_path(path);
    if (failures != 0) {
        return failures;
    }

    {
        mylite_db *database = NULL;
        mylite_result *result = NULL;
        enum ddl_fault_state state = ddl_fault_state_invalid;
        int close_rc = MYLITE_OK;

        failures +=
            mylite_test_expect_int(open_model_database(path, &database), MYLITE_OK, scenario->name);
        failures += setup_ddl_fault_scenario(database, scenario);
        failures += query_catalog_generation(database, &expected_pre_generation);
        if (use_truncate_journal) {
            failures += configure_truncate_journal(database);
        }
        mylite_storage_vfs_test_set_fault(operation, SIZE_MAX);
        failures += mylite_test_expect_int(
            mylite_execute(database, scenario->sql, strlen(scenario->sql), &result),
            MYLITE_OK,
            "measure non-faulted DDL"
        );
        mylite_result_free(result);
        failures += classify_ddl_fault_state(database, scenario, &state);
        failures += mylite_test_expect_int(
            state,
            ddl_fault_state_post,
            "measurement DDL reaches complete post-state"
        );
        failures += query_catalog_generation(database, &expected_post_generation);
        close_rc = close_after_fault(database);
        database = NULL;
        failures += expect_true(
            !mylite_storage_vfs_test_fault_was_triggered(),
            "measurement DDL does not trigger fault"
        );
        operation_call_count = mylite_storage_vfs_test_matching_call_count();
        failures += expect_true(operation_call_count > 0U, "DDL reaches measured VFS operation");
        mylite_storage_vfs_test_clear_fault();
        failures +=
            mylite_test_expect_int(close_rc, MYLITE_OK, "measurement database close succeeds");
    }

    for (size_t fail_on_call = 1U; fail_on_call <= operation_call_count; ++fail_on_call) {
        mylite_db *database = NULL;
        mylite_result *result = NULL;
        enum ddl_fault_state reopened_state = ddl_fault_state_invalid;
        enum ddl_fault_state second_reopen_state = ddl_fault_state_invalid;
        bool fault_triggered = false;
        int recovered_generation = 0;

        remove_related_files(path);
        failures +=
            mylite_test_expect_int(open_model_database(path, &database), MYLITE_OK, scenario->name);
        failures += setup_ddl_fault_scenario(database, scenario);
        if (use_truncate_journal) {
            failures += configure_truncate_journal(database);
        }
        mylite_storage_vfs_test_set_fault(operation, fail_on_call);
        (void)mylite_execute(database, scenario->sql, strlen(scenario->sql), &result);
        mylite_result_free(result);
        (void)close_after_fault(database);
        database = NULL;
        fault_triggered = mylite_storage_vfs_test_fault_was_triggered();
        mylite_storage_vfs_test_clear_fault();

        failures += mylite_test_expect_int(
            open_model_database(path, &database),
            MYLITE_OK,
            "reopen DDL fault file"
        );
        failures += classify_ddl_fault_state(database, scenario, &reopened_state);
        failures += expect_true(
            reopened_state == ddl_fault_state_pre || reopened_state == ddl_fault_state_post,
            "faulted DDL reopens in a complete pre or post state"
        );
        failures += query_catalog_generation(database, &recovered_generation);
        failures += expect_true(
            (reopened_state == ddl_fault_state_pre &&
             recovered_generation == expected_pre_generation) ||
                (reopened_state == ddl_fault_state_post &&
                 recovered_generation == expected_post_generation),
            "faulted DDL generation matches the classified state"
        );
        failures += assert_recovery_invariants(path, database);
        failures += expect_true(fault_triggered, "measured DDL fault is triggered");
        failures += reopen_database(path, &database);
        failures += classify_ddl_fault_state(database, scenario, &second_reopen_state);
        failures += mylite_test_expect_int(
            second_reopen_state,
            reopened_state,
            "second reopen preserves the recovered DDL state"
        );
        failures += assert_recovery_invariants(path, database);
        mylite_close(database);
    }

    remove_related_files(path);
    return failures;
}

static int setup_ddl_fault_scenario(
    mylite_db *database,
    const struct ddl_fault_scenario *scenario
) {
    int failures = 0;

    if (scenario->kind == ddl_fault_create_table) {
        return 0;
    }
    failures += execute_ok(
        database,
        "CREATE TABLE fault_table(id INT NOT NULL PRIMARY KEY, value INT NOT NULL)"
    );
    failures += execute_ok(database, "INSERT INTO fault_table VALUES (1, 11), (2, 22)");
    if (scenario->kind == ddl_fault_drop_index) {
        failures += execute_ok(database, "CREATE INDEX fault_index ON fault_table(value)");
    }
    return failures;
}

static int classify_ddl_fault_state(
    mylite_db *database,
    const struct ddl_fault_scenario *scenario,
    enum ddl_fault_state *out_state
) {
    sqlite3 *sqlite = mylite_connection_sqlite_for_test(database);
    const char *row_table_name = "fault_table";
    struct ddl_fault_observation observation = {0};
    int failures = 0;

    *out_state = ddl_fault_state_invalid;
    if (sqlite == NULL) {
        return 1;
    }
    failures += query_single_int(
        sqlite,
        "SELECT count(*) FROM _mylite_catalog_tables WHERE name = 'fault_table'",
        &observation.old_table_count
    );
    failures += query_single_int(
        sqlite,
        "SELECT count(*) FROM _mylite_catalog_tables WHERE name = 'fault_table_renamed'",
        &observation.renamed_table_count
    );
    failures += query_single_int(
        sqlite,
        "SELECT count(*) FROM _mylite_catalog_columns AS c "
        "JOIN _mylite_catalog_tables AS t ON t.table_id = c.table_id "
        "WHERE t.name IN ('fault_table', 'fault_table_renamed')",
        &observation.column_count
    );
    failures += query_single_int(
        sqlite,
        "SELECT count(*) FROM _mylite_catalog_columns AS c "
        "JOIN _mylite_catalog_tables AS t ON t.table_id = c.table_id "
        "WHERE t.name IN ('fault_table', 'fault_table_renamed') "
        "AND c.name = 'value' AND c.logical_type = 'BIGINT'",
        &observation.bigint_column_count
    );
    failures += query_single_int(
        sqlite,
        "SELECT count(*) FROM _mylite_catalog_indexes AS i "
        "JOIN _mylite_catalog_tables AS t ON t.table_id = i.table_id "
        "WHERE t.name IN ('fault_table', 'fault_table_renamed') "
        "AND i.name = 'fault_index'",
        &observation.index_count
    );
    if (failures != 0) {
        return failures;
    }

    if (observation.renamed_table_count == 1 && observation.old_table_count == 0) {
        row_table_name = "fault_table_renamed";
    }
    if (observation.old_table_count + observation.renamed_table_count == 1) {
        failures += query_fault_table_rows(database, row_table_name, &observation.rows);
    }
    if (failures != 0) {
        return failures;
    }

    switch (scenario->kind) {
    case ddl_fault_create_table:
        *out_state = classify_create_table(&observation);
        break;
    case ddl_fault_modify_column:
        *out_state = classify_modify_column(&observation);
        break;
    case ddl_fault_drop_table:
        *out_state = classify_drop_table(&observation);
        break;
    case ddl_fault_rename_table:
        *out_state = classify_rename_table(&observation);
        break;
    case ddl_fault_create_index:
        *out_state = classify_create_index(&observation);
        break;
    case ddl_fault_drop_index:
        *out_state = classify_drop_index(&observation);
        break;
    case ddl_fault_truncate_table:
        *out_state = classify_truncate_table(&observation);
        break;
    }
    return failures;
}

static enum ddl_fault_state classify_create_table(const struct ddl_fault_observation *observation) {
    if (observation->old_table_count == 0 && observation->renamed_table_count == 0 &&
        observation->column_count == 0 && observation->bigint_column_count == 0 &&
        observation->index_count == 0 && ddl_fault_has_no_rows(observation)) {
        return ddl_fault_state_pre;
    }
    if (ddl_fault_has_original_int_table(observation) && observation->index_count == 0 &&
        ddl_fault_has_no_rows(observation)) {
        return ddl_fault_state_post;
    }
    return ddl_fault_state_invalid;
}

static enum ddl_fault_state classify_modify_column(const struct ddl_fault_observation *state) {
    if (state->old_table_count != 1 || state->renamed_table_count != 0 ||
        state->column_count != ddl_fault_table_column_count || state->index_count != 0 ||
        !ddl_fault_has_setup_rows(state)) {
        return ddl_fault_state_invalid;
    }
    if (state->bigint_column_count == 0) {
        return ddl_fault_state_pre;
    }
    if (state->bigint_column_count == 1) {
        return ddl_fault_state_post;
    }
    return ddl_fault_state_invalid;
}

static enum ddl_fault_state classify_drop_table(const struct ddl_fault_observation *observation) {
    if (ddl_fault_has_original_int_table(observation) && observation->index_count == 0 &&
        ddl_fault_has_setup_rows(observation)) {
        return ddl_fault_state_pre;
    }
    if (observation->old_table_count == 0 && observation->renamed_table_count == 0 &&
        observation->column_count == 0 && observation->bigint_column_count == 0 &&
        observation->index_count == 0 && ddl_fault_has_no_rows(observation)) {
        return ddl_fault_state_post;
    }
    return ddl_fault_state_invalid;
}

static enum ddl_fault_state classify_rename_table(const struct ddl_fault_observation *observation) {
    if (ddl_fault_has_original_int_table(observation) && observation->index_count == 0 &&
        ddl_fault_has_setup_rows(observation)) {
        return ddl_fault_state_pre;
    }
    if (ddl_fault_has_renamed_int_table(observation) && observation->index_count == 0 &&
        ddl_fault_has_setup_rows(observation)) {
        return ddl_fault_state_post;
    }
    return ddl_fault_state_invalid;
}

static enum ddl_fault_state classify_create_index(const struct ddl_fault_observation *observation) {
    if (!ddl_fault_has_original_int_table(observation) || !ddl_fault_has_setup_rows(observation)) {
        return ddl_fault_state_invalid;
    }
    if (observation->index_count == 0) {
        return ddl_fault_state_pre;
    }
    if (observation->index_count == 1) {
        return ddl_fault_state_post;
    }
    return ddl_fault_state_invalid;
}

static enum ddl_fault_state classify_drop_index(const struct ddl_fault_observation *observation) {
    if (!ddl_fault_has_original_int_table(observation) || !ddl_fault_has_setup_rows(observation)) {
        return ddl_fault_state_invalid;
    }
    if (observation->index_count == 1) {
        return ddl_fault_state_pre;
    }
    if (observation->index_count == 0) {
        return ddl_fault_state_post;
    }
    return ddl_fault_state_invalid;
}

static enum ddl_fault_state classify_truncate_table(const struct ddl_fault_observation *state) {
    if (!ddl_fault_has_original_int_table(state) || state->index_count != 0) {
        return ddl_fault_state_invalid;
    }
    if (ddl_fault_has_setup_rows(state)) {
        return ddl_fault_state_pre;
    }
    if (ddl_fault_has_no_rows(state)) {
        return ddl_fault_state_post;
    }
    return ddl_fault_state_invalid;
}

static bool ddl_fault_has_original_int_table(const struct ddl_fault_observation *observation) {
    return observation->old_table_count == 1 && observation->renamed_table_count == 0 &&
           observation->column_count == ddl_fault_table_column_count &&
           observation->bigint_column_count == 0;
}

static bool ddl_fault_has_renamed_int_table(const struct ddl_fault_observation *observation) {
    return observation->old_table_count == 0 && observation->renamed_table_count == 1 &&
           observation->column_count == ddl_fault_table_column_count &&
           observation->bigint_column_count == 0;
}

static bool ddl_fault_has_setup_rows(const struct ddl_fault_observation *observation) {
    return observation->rows.count == ddl_fault_setup_row_count &&
           observation->rows.id_sum == ddl_fault_setup_id_sum &&
           observation->rows.value_sum == ddl_fault_setup_value_sum;
}

static bool ddl_fault_has_no_rows(const struct ddl_fault_observation *observation) {
    return observation->rows.count == 0 && observation->rows.id_sum == 0 &&
           observation->rows.value_sum == 0;
}

static int query_fault_table_rows(
    mylite_db *database,
    const char *table_name,
    struct fault_table_rows *out_rows
) {
    enum { physical_name_capacity = 128 };

    sqlite3 *sqlite = mylite_connection_sqlite_for_test(database);
    char physical_name[physical_name_capacity];
    char sql[sql_capacity];
    int failures = 0;
    int written = 0;

    if (sqlite == NULL) {
        return 1;
    }
    written = snprintf(
        sql,
        sizeof(sql),
        "SELECT physical_name FROM _mylite_catalog_tables WHERE name = '%s'",
        table_name
    );
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        return 1;
    }
    failures += query_single_text_value(sqlite, sql, physical_name, sizeof(physical_name));
    if (failures != 0) {
        return failures;
    }
    written = snprintf(
        sql,
        sizeof(sql),
        "SELECT count(*), COALESCE(SUM(id), 0), COALESCE(SUM(value), 0) FROM \"%s\"",
        physical_name
    );
    if (written < 0 || (size_t)written >= sizeof(sql)) {
        return failures + 1;
    }
    failures += query_sqlite_ints(sqlite, sql, out_rows);
    return failures;
}

static int assert_recovery_invariants(const char *path, mylite_db *database) {
    sqlite3 *sqlite = mylite_connection_sqlite_for_test(database);
    bool seal_matches = false;
    int scratch_object_count = -1;
    int failures = 0;

    if (sqlite == NULL) {
        return 1;
    }
    failures += query_single_text(sqlite, "PRAGMA integrity_check", "ok");
    failures += query_integrity_seal_matches(sqlite, &seal_matches);
    failures += expect_true(seal_matches, "recovered database integrity seal matches");
    failures += query_single_int(
        sqlite,
        "SELECT count(*) FROM sqlite_schema WHERE "
        "name GLOB '_mylite_user_table_*_modify_*' OR "
        "name GLOB '_mylite_user_table_*_order_*' OR "
        "name GLOB '_mylite_user_table_*_force_*' OR "
        "name GLOB '_mylite_user_table_*_check_*' OR "
        "name GLOB '_mylite_catalog_*_v[0-9]*'",
        &scratch_object_count
    );
    failures += mylite_test_expect_int(
        scratch_object_count,
        0,
        "recovered database has no persistent scratch objects"
    );
    failures += mylite_test_expect_int(
        file_exists_with_suffix(path, "-journal"),
        0,
        "recovered database has no rollback journal"
    );
    return failures;
}

static int query_catalog_generation(mylite_db *database, int *out_generation) {
    sqlite3 *sqlite = mylite_connection_sqlite_for_test(database);

    if (sqlite == NULL) {
        return 1;
    }
    return query_single_int(
        sqlite,
        "SELECT catalog_generation FROM _mylite_catalog_state",
        out_generation
    );
}

static int query_integrity_seal_matches(sqlite3 *sqlite, bool *out_matches) {
    int catalog_generation = 0;
    int integrity_generation = 0;
    int integrity_schema_version = 0;
    int sqlite_schema_version = 0;
    int failures = 0;

    *out_matches = false;
    failures += query_single_int(
        sqlite,
        "SELECT catalog_generation FROM _mylite_catalog_state",
        &catalog_generation
    );
    failures += query_single_int(
        sqlite,
        "SELECT integrity_catalog_generation FROM _mylite_catalog_state",
        &integrity_generation
    );
    failures += query_single_int(
        sqlite,
        "SELECT integrity_sqlite_schema_version FROM _mylite_catalog_state",
        &integrity_schema_version
    );
    failures += query_single_int(sqlite, "PRAGMA main.schema_version", &sqlite_schema_version);
    if (failures == 0) {
        *out_matches = integrity_generation != 0 && integrity_generation == catalog_generation &&
                       integrity_schema_version == sqlite_schema_version;
    }
    return failures;
}

static int configure_truncate_journal(mylite_db *database) {
    sqlite3 *sqlite = mylite_connection_sqlite_for_test(database);

    if (sqlite == NULL) {
        return 1;
    }
    return query_single_text(sqlite, "PRAGMA journal_mode=TRUNCATE", "truncate");
}

static int close_after_fault(mylite_db *database) {
    int rc = mylite_close_checked(database);

    if (rc != MYLITE_OK) {
        mylite_close(database);
    }
    return rc;
}

static int test_catalog_migration_fault_atomicity(void) {
    int failures = 0;

    failures += run_catalog_migration_fault_matrix(MYLITE_STORAGE_VFS_FAULT_WRITE, "write");
    failures += run_catalog_migration_fault_matrix(MYLITE_STORAGE_VFS_FAULT_SYNC, "sync");
    failures += run_catalog_migration_fault_matrix(MYLITE_STORAGE_VFS_FAULT_TRUNCATE, "truncate");
    failures += run_catalog_migration_fault_matrix(MYLITE_STORAGE_VFS_FAULT_DELETE, "delete");
    failures += run_catalog_migration_fault_matrix(MYLITE_STORAGE_VFS_FAULT_CLOSE, "close");
    return failures;
}

static int run_catalog_migration_fault_matrix(
    enum mylite_storage_vfs_fault_operation operation,
    const char *operation_name
) {
    char path[path_capacity];
    size_t operation_call_count = 0U;
    bool use_truncate_journal = operation == MYLITE_STORAGE_VFS_FAULT_TRUNCATE;
    int failures = prepare_path(path);

    if (failures != 0) {
        return failures;
    }

    {
        mylite_db *database = NULL;
        int close_rc = MYLITE_OK;

        failures += setup_downgraded_catalog(path, use_truncate_journal);
        mylite_storage_test_set_truncate_journal(use_truncate_journal);
        mylite_storage_vfs_test_set_fault(operation, SIZE_MAX);
        failures += mylite_test_expect_int(
            mylite_open(path, &database),
            MYLITE_OK,
            "measure non-faulted catalog migration"
        );
        close_rc = close_after_fault(database);
        database = NULL;
        failures += expect_true(
            !mylite_storage_vfs_test_fault_was_triggered(),
            "measurement migration does not trigger fault"
        );
        operation_call_count = mylite_storage_vfs_test_matching_call_count();
        failures += expect_true(
            operation_call_count > 0U,
            "catalog migration reaches measured VFS operation"
        );
        mylite_storage_vfs_test_clear_fault();
        mylite_storage_test_set_truncate_journal(false);
        failures += mylite_test_expect_int(
            close_rc,
            MYLITE_OK,
            "measurement migration database close succeeds"
        );
    }

    for (size_t fail_on_call = 1U; fail_on_call <= operation_call_count; ++fail_on_call) {
        mylite_db *database = NULL;
        sqlite3 *raw_sqlite = NULL;
        enum ddl_fault_state migration_state = ddl_fault_state_invalid;
        struct fault_table_rows rows = {0};
        bool fault_triggered = false;

        failures += setup_downgraded_catalog(path, use_truncate_journal);
        mylite_storage_test_set_truncate_journal(use_truncate_journal);
        mylite_storage_vfs_test_set_fault(operation, fail_on_call);
        (void)mylite_open(path, &database);
        if (database != NULL) {
            (void)close_after_fault(database);
            database = NULL;
        }
        fault_triggered = mylite_storage_vfs_test_fault_was_triggered();
        mylite_storage_vfs_test_clear_fault();
        mylite_storage_test_set_truncate_journal(false);
        failures += expect_true(fault_triggered, operation_name);

        failures += mylite_test_expect_int(
            mylite_storage_open_sqlite_payload(path, &raw_sqlite),
            MYLITE_OK,
            "open faulted catalog migration without remigrating"
        );
        if (raw_sqlite != NULL) {
            failures += classify_catalog_migration_state(raw_sqlite, &migration_state);
            failures += query_single_text(raw_sqlite, "PRAGMA integrity_check", "ok");
            failures += mylite_test_expect_int(
                sqlite3_close(raw_sqlite),
                SQLITE_OK,
                "close raw migration inspection"
            );
            raw_sqlite = NULL;
        }
        failures += expect_true(
            migration_state == ddl_fault_state_pre || migration_state == ddl_fault_state_post,
            "faulted catalog migration is complete pre or post state"
        );

        failures += mylite_test_expect_int(
            open_model_database(path, &database),
            MYLITE_OK,
            "converge faulted catalog migration"
        );
        failures += assert_recovery_invariants(path, database);
        failures += query_fault_table_rows(database, "fault_table", &rows);
        failures += mylite_test_expect_int(
            rows.count,
            ddl_fault_setup_row_count,
            "migration recovery preserves row count"
        );
        failures += mylite_test_expect_int(
            rows.id_sum,
            ddl_fault_setup_id_sum,
            "migration recovery preserves row ids"
        );
        failures += mylite_test_expect_int(
            rows.value_sum,
            ddl_fault_setup_value_sum,
            "migration recovery preserves values"
        );
        mylite_close(database);
    }

    remove_related_files(path);
    return failures;
}

static int test_catalog_migration_process_death(const char *executable_path) {
    int failures = 0;

    failures += run_catalog_migration_process_death_matrix(
        executable_path,
        MYLITE_STORAGE_VFS_FAULT_WRITE,
        "write"
    );
    failures += run_catalog_migration_process_death_matrix(
        executable_path,
        MYLITE_STORAGE_VFS_FAULT_SYNC,
        "sync"
    );
    failures += run_catalog_migration_process_death_matrix(
        executable_path,
        MYLITE_STORAGE_VFS_FAULT_TRUNCATE,
        "truncate"
    );
    return failures;
}

static int run_catalog_migration_process_death_matrix(
    const char *executable_path,
    enum mylite_storage_vfs_fault_operation operation,
    const char *operation_name
) {
    char path[path_capacity];
    size_t operation_call_count = 0U;
    bool use_truncate_journal = operation == MYLITE_STORAGE_VFS_FAULT_TRUNCATE;
    int failures = prepare_path(path);

    if (failures != 0) {
        return failures;
    }

    {
        mylite_db *database = NULL;
        int close_rc = MYLITE_OK;

        failures += setup_downgraded_catalog(path, use_truncate_journal);
        mylite_storage_test_set_truncate_journal(use_truncate_journal);
        mylite_storage_vfs_test_set_fault(operation, SIZE_MAX);
        failures += mylite_test_expect_int(
            mylite_open(path, &database),
            MYLITE_OK,
            "measure migration process-death boundary"
        );
        close_rc = close_after_fault(database);
        database = NULL;
        operation_call_count = mylite_storage_vfs_test_matching_call_count();
        failures += expect_true(
            !mylite_storage_vfs_test_fault_was_triggered(),
            "migration process-death measurement does not trigger"
        );
        failures += expect_true(
            operation_call_count > 0U,
            "migration reaches measured process-death boundary"
        );
        mylite_storage_vfs_test_clear_fault();
        mylite_storage_test_set_truncate_journal(false);
        failures += mylite_test_expect_int(
            close_rc,
            MYLITE_OK,
            "migration process-death measurement closes"
        );
    }

    for (size_t exit_on_call = 1U; exit_on_call <= operation_call_count; ++exit_on_call) {
        mylite_db *database = NULL;
        sqlite3 *raw_sqlite = NULL;
        enum ddl_fault_state migration_state = ddl_fault_state_invalid;
        struct fault_table_rows rows = {0};

        failures += setup_downgraded_catalog(path, use_truncate_journal);
        failures += run_migration_process_death_child(
            executable_path,
            path,
            operation,
            operation_name,
            exit_on_call
        );

        failures += mylite_test_expect_int(
            mylite_storage_open_sqlite_payload(path, &raw_sqlite),
            MYLITE_OK,
            "open crashed catalog migration without remigrating"
        );
        if (raw_sqlite != NULL) {
            failures += classify_catalog_migration_state(raw_sqlite, &migration_state);
            failures += query_single_text(raw_sqlite, "PRAGMA integrity_check", "ok");
            failures += mylite_test_expect_int(
                sqlite3_close(raw_sqlite),
                SQLITE_OK,
                "close crashed migration inspection"
            );
            raw_sqlite = NULL;
        }
        failures += expect_true(
            migration_state == ddl_fault_state_pre || migration_state == ddl_fault_state_post,
            "crashed catalog migration is complete pre or post state"
        );

        failures += mylite_test_expect_int(
            open_model_database(path, &database),
            MYLITE_OK,
            "converge crashed catalog migration"
        );
        failures += assert_recovery_invariants(path, database);
        failures += query_fault_table_rows(database, "fault_table", &rows);
        failures += mylite_test_expect_int(
            rows.count,
            ddl_fault_setup_row_count,
            "crashed migration preserves row count"
        );
        failures += mylite_test_expect_int(
            rows.id_sum,
            ddl_fault_setup_id_sum,
            "crashed migration preserves row ids"
        );
        failures += mylite_test_expect_int(
            rows.value_sum,
            ddl_fault_setup_value_sum,
            "crashed migration preserves values"
        );
        mylite_close(database);
    }

    remove_related_files(path);
    return failures;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
static int run_migration_process_death_child(
    const char *executable_path,
    const char *path,
    enum mylite_storage_vfs_fault_operation operation,
    const char *operation_name,
    size_t exit_on_call
) {
#ifdef _WIN32
    char exit_on_call_text[integer_text_capacity];
    intptr_t child_status = -1;
    int written = snprintf(exit_on_call_text, sizeof(exit_on_call_text), "%zu", exit_on_call);

    (void)operation;
    if (written < 0 || (size_t)written >= sizeof(exit_on_call_text)) {
        return 1;
    }
    child_status = _spawnl(
        _P_WAIT,
        executable_path,
        executable_path,
        "--migration-death-child",
        path,
        operation_name,
        exit_on_call_text,
        NULL
    );
    if (child_status == -1) {
        fprintf(stderr, "spawn migration process-death child failed\n");
        return 1;
    }
    return mylite_test_expect_int(
        (int)child_status,
        0,
        "migration child reached process-death boundary"
    );
#else
    pid_t child = fork();
    int child_status = 0;

    (void)executable_path;
    (void)operation_name;
    if (child == 0) {
        _exit(migration_process_death_child_main(path, operation, exit_on_call));
    }
    if (child < 0) {
        fprintf(stderr, "fork migration process-death child failed\n");
        return 1;
    }
    if (waitpid(child, &child_status, 0) != child) {
        fprintf(stderr, "wait for migration process-death child failed\n");
        return 1;
    }
    if (!WIFEXITED(child_status)) {
        fprintf(stderr, "migration process-death child did not exit normally\n");
        return 1;
    }
    return mylite_test_expect_int(
        WEXITSTATUS(child_status),
        0,
        "migration child reached process-death boundary"
    );
#endif
}

// NOLINTEND(bugprone-easily-swappable-parameters)

static int migration_process_death_child_main(
    const char *path,
    enum mylite_storage_vfs_fault_operation operation,
    size_t exit_on_call
) {
    mylite_db *database = NULL;

    mylite_storage_test_set_truncate_journal(operation == MYLITE_STORAGE_VFS_FAULT_TRUNCATE);
    mylite_storage_vfs_test_set_process_death(operation, exit_on_call);
    (void)mylite_open(path, &database);
    mylite_close(database);
    mylite_storage_vfs_test_clear_fault();
    mylite_storage_test_set_truncate_journal(false);
    return 2;
}

static int parse_migration_process_death_operation(
    const char *text,
    enum mylite_storage_vfs_fault_operation *out_operation
) {
    *out_operation = MYLITE_STORAGE_VFS_FAULT_NONE;
    if (strcmp(text, "write") == 0) {
        *out_operation = MYLITE_STORAGE_VFS_FAULT_WRITE;
    } else if (strcmp(text, "sync") == 0) {
        *out_operation = MYLITE_STORAGE_VFS_FAULT_SYNC;
    } else if (strcmp(text, "truncate") == 0) {
        *out_operation = MYLITE_STORAGE_VFS_FAULT_TRUNCATE;
    }
    return *out_operation == MYLITE_STORAGE_VFS_FAULT_NONE ? 1 : 0;
}

static int parse_positive_size(const char *text, size_t *out_value) {
    char *end = NULL;
    unsigned long long parsed = 0U;

    errno = 0;
    parsed = strtoull(text, &end, decimal_radix);
    if (errno != 0 || end == text || *end != '\0' || parsed == 0U ||
        parsed > (unsigned long long)SIZE_MAX) {
        return 1;
    }
    *out_value = (size_t)parsed;
    return 0;
}

static int setup_downgraded_catalog(const char *path, bool use_truncate_journal) {
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    char sql[sql_capacity];
    int failures = 0;
    int written = 0;

    remove_related_files(path);
    failures += mylite_test_expect_int(
        open_model_database(path, &database),
        MYLITE_OK,
        "create catalog migration source"
    );
    failures += execute_ok(
        database,
        "CREATE TABLE fault_table(id INT NOT NULL PRIMARY KEY, value INT NOT NULL)"
    );
    failures += execute_ok(database, "INSERT INTO fault_table VALUES (1, 11), (2, 22)");
    if (use_truncate_journal) {
        failures += configure_truncate_journal(database);
    }
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite == NULL) {
        failures += 1;
    } else {
        failures += mylite_test_remove_catalog_integrity_seal(sqlite);
        if (failures == 0) {
            written = snprintf(
                sql,
                sizeof(sql),
                "UPDATE _mylite_catalog_state "
                "SET schema_version = %d, minimum_reader_schema_version = %d",
                catalog_previous_schema_version,
                catalog_previous_schema_version
            );
            if (written < 0 || (size_t)written >= sizeof(sql) ||
                sqlite3_exec(sqlite, sql, NULL, NULL, NULL) != SQLITE_OK) {
                failures += 1;
            }
        }
    }
    mylite_close(database);
    return failures;
}

static int classify_catalog_migration_state(sqlite3 *sqlite, enum ddl_fault_state *out_state) {
    int schema_version = 0;
    int integrity_column_count = 0;
    int integrity_trigger_count = 0;
    int scratch_object_count = 0;
    int failures = 0;

    *out_state = ddl_fault_state_invalid;
    failures += query_single_int(
        sqlite,
        "SELECT schema_version FROM _mylite_catalog_state",
        &schema_version
    );
    failures += query_single_int(
        sqlite,
        "SELECT count(*) FROM pragma_table_info('_mylite_catalog_state') "
        "WHERE name IN ('integrity_catalog_generation', 'integrity_sqlite_schema_version')",
        &integrity_column_count
    );
    failures += query_single_int(
        sqlite,
        "SELECT count(*) FROM sqlite_schema WHERE type = 'trigger' "
        "AND name GLOB '_mylite_integrity_*'",
        &integrity_trigger_count
    );
    failures += query_single_int(
        sqlite,
        "SELECT count(*) FROM sqlite_schema WHERE name = '_mylite_catalog_state_v38'",
        &scratch_object_count
    );
    if (failures != 0 || scratch_object_count != 0) {
        return failures;
    }
    if (schema_version == catalog_previous_schema_version && integrity_column_count == 0 &&
        integrity_trigger_count == 0) {
        *out_state = ddl_fault_state_pre;
    } else if (schema_version == MYLITE_CATALOG_SCHEMA_VERSION &&
               integrity_column_count == catalog_integrity_column_count &&
               integrity_trigger_count > 0) {
        *out_state = ddl_fault_state_post;
    }
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

static int file_exists_with_suffix(const char *path, const char *suffix) {
    char related_path[path_capacity + path_suffix_capacity];
    FILE *file = NULL;
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return 0;
    }
    file = fopen(related_path, "rb");
    if (file == NULL) {
        return 0;
    }
    (void)fclose(file);
    return 1;
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

static int query_single_text_value(
    sqlite3 *sqlite,
    const char *sql,
    char *destination,
    size_t destination_size
) {
    sqlite3_stmt *statement = NULL;
    const unsigned char *value = NULL;
    int rc = sqlite3_prepare_v2(sqlite, sql, -1, &statement, NULL);
    int failures = 0;
    int written = 0;

    destination[0] = '\0';
    if (rc != SQLITE_OK || sqlite3_step(statement) != SQLITE_ROW) {
        fprintf(stderr, "%s: SQLite text query failed: %s\n", sql, sqlite3_errmsg(sqlite));
        sqlite3_finalize(statement);
        return 1;
    }
    value = sqlite3_column_text(statement, 0);
    if (value == NULL) {
        failures += 1;
    } else {
        written = snprintf(destination, destination_size, "%s", (const char *)value);
        if (written < 0 || (size_t)written >= destination_size) {
            failures += 1;
        }
    }
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        failures += 1;
    }
    return failures;
}

static int query_sqlite_ints(
    sqlite3 *sqlite,
    const char *sql,
    struct fault_table_rows *out_values
) {
    sqlite3_stmt *statement = NULL;
    int rc = sqlite3_prepare_v2(sqlite, sql, -1, &statement, NULL);
    int failures = 0;

    if (rc != SQLITE_OK || sqlite3_step(statement) != SQLITE_ROW) {
        fprintf(stderr, "%s: SQLite integer query failed: %s\n", sql, sqlite3_errmsg(sqlite));
        sqlite3_finalize(statement);
        return 1;
    }
    out_values->count = sqlite3_column_int(statement, 0);
    out_values->id_sum = sqlite3_column_int(statement, 1);
    out_values->value_sum = sqlite3_column_int(statement, 2);
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        failures += 1;
    }
    return failures;
}

static int expect_true(bool condition, const char *context) {
    if (condition) {
        return 0;
    }
    fprintf(stderr, "%s: expected true\n", context);
    return 1;
}
