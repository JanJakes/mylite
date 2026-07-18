#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "sqlite3.h"
#include "storage/mylite_file_format.h"
#include "storage/mylite_file_open.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <windows.h>

#  include <process.h>
#  include <sys/stat.h>
#else
#  include <sys/stat.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    sqlite_header_size = 16,
    reopen_marker_value = 73,
    updated_reopen_marker_value = 74,
};

static int test_open_rejects_invalid_arguments(void);
static int test_create_new_file_with_preamble_and_shifted_payload(void);
static int test_reopen_existing_file_preserves_sqlite_payload(void);
static int test_rejects_invalid_truncated_and_plain_sqlite_files(void);
static int test_independent_file_backed_handles_and_bootstrap_state(void);
static int test_reopens_legacy_version_one_file(void);
static int test_rejects_incomplete_lifecycle_files(void);
static int test_rejects_second_opener_during_initialization(void);
static int test_rejects_concurrent_process_opener(const char *executable_path);
static int test_vfs_fault_injection(void);
static int test_lock_byte_gap_mapping(void);
static int test_legacy_lock_boundary_containment(void);
static int test_journal_mode_policy(void);
#ifndef _WIN32
static int test_process_death_leaves_initializing_file_rejected(void);
static int test_abort_marks_opened_identity_recovery_required(void);
static int test_hot_journal_recovery_after_process_death(void);
#endif
static int test_symlink_failure_preserves_path_identity(void);
static int create_file_symlink(const char *target_path, const char *link_path);
static int path_is_symlink(const char *path);
static int initialization_child_main(
    const char *path,
    const char *ready_path,
    const char *release_path
);
static int wait_for_path(const char *path);
static int path_exists(const char *path);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int write_file_bytes(const char *path, const void *bytes, size_t size);
static int write_file_at(const char *path, long offset, const void *bytes, size_t size);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int file_size(const char *path, long *out_size);
static int file_exists_with_suffix(const char *path, const char *suffix);
static int create_plain_sqlite_database(const char *path);
static int execute_sql(sqlite3 *connection, const char *sql);
static int query_single_int(sqlite3 *connection, const char *sql, int *out_value);
static int query_single_text_equals(sqlite3 *connection, const char *sql, const char *expected);
static int table_exists(sqlite3 *connection, const char *table_name, int *out_exists);
static int expect_int(int actual, int expected, const char *context);
static int expect_long(long actual, long expected, const char *context);
static int expect_int64(sqlite3_int64 actual, sqlite3_int64 expected, const char *context);
static int expect_bool(bool actual, bool expected, const char *context);
static int expect_true(int condition, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);
static int expect_not_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(int argc, char **argv) {
    int failures = 0;

    if (argc == 5 && strcmp(argv[1], "--initialization-child") == 0) {
        return initialization_child_main(argv[2], argv[3], argv[4]);
    }

    failures += test_open_rejects_invalid_arguments();
    failures += test_create_new_file_with_preamble_and_shifted_payload();
    failures += test_reopen_existing_file_preserves_sqlite_payload();
    failures += test_rejects_invalid_truncated_and_plain_sqlite_files();
    failures += test_independent_file_backed_handles_and_bootstrap_state();
    failures += test_reopens_legacy_version_one_file();
    failures += test_rejects_incomplete_lifecycle_files();
    failures += test_rejects_second_opener_during_initialization();
    failures += test_rejects_concurrent_process_opener(argv[0]);
    failures += test_vfs_fault_injection();
    failures += test_lock_byte_gap_mapping();
    failures += test_legacy_lock_boundary_containment();
    failures += test_journal_mode_policy();
#ifndef _WIN32
    failures += test_process_death_leaves_initializing_file_rejected();
    failures += test_abort_marks_opened_identity_recovery_required();
    failures += test_hot_journal_recovery_after_process_death();
#endif
    failures += test_symlink_failure_preserves_path_identity();

    return failures == 0 ? 0 : 1;
}

static int test_open_rejects_invalid_arguments(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open(NULL, &database), MYLITE_MISUSE, "reject NULL path");
    failures += expect_true(database == NULL, "NULL path leaves output null");
    failures += expect_int(mylite_open("", &database), MYLITE_MISUSE, "reject empty path");
    failures += expect_true(database == NULL, "empty path leaves output null");
    failures += expect_int(mylite_open("unused.mylite", NULL), MYLITE_MISUSE, "reject NULL output");

    return failures;
}

static int test_create_new_file_with_preamble_and_shifted_payload(void) {
    static const unsigned char sqlite_header[sqlite_header_size] = "SQLite format 3";

    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char payload_header[sqlite_header_size];
    int failures = 0;

    if (make_test_path(path, sizeof(path), "create") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open new file-backed handle");
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += expect_true(sqlite != NULL, "file-backed SQLite connection exists");
    if (sqlite != NULL) {
        failures += execute_sql(
            sqlite,
            "CREATE TABLE shifted_payload(value INTEGER);"
            "INSERT INTO shifted_payload(value) VALUES (42)"
        );
    }
    mylite_close(database);

    failures += read_file_at(path, 0L, preamble, sizeof(preamble));
    failures +=
        expect_int(mylite_file_preamble_validate(preamble), 1, "created preamble validates");
    failures += read_file_at(
        path,
        MYLITE_FILE_SQLITE_PAYLOAD_OFFSET,
        payload_header,
        sizeof(payload_header)
    );
    failures +=
        expect_bytes(payload_header, sqlite_header, sizeof(sqlite_header), "shifted SQLite header");
    failures +=
        expect_not_bytes(preamble, sqlite_header, sizeof(sqlite_header), "byte 0 is not SQLite");

    remove_related_files(path);

    return failures;
}

static int test_reopen_existing_file_preserves_sqlite_payload(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int stored_value = 0;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "reopen") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file to populate");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += execute_sql(
            sqlite,
            "CREATE TABLE reopen_marker(value INTEGER);"
            "INSERT INTO reopen_marker(value) VALUES (73)"
        );
    }
    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen existing file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += query_single_int(sqlite, "SELECT value FROM reopen_marker", &stored_value);
    }
    failures += expect_int(stored_value, reopen_marker_value, "reopened payload preserves row");
    if (sqlite != NULL) {
        failures += execute_sql(sqlite, "UPDATE reopen_marker SET value = 74");
        failures += query_single_int(sqlite, "SELECT value FROM reopen_marker", &stored_value);
    }
    failures +=
        expect_int(stored_value, updated_reopen_marker_value, "reopened payload remains writable");

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_rejects_invalid_truncated_and_plain_sqlite_files(void) {
    static const unsigned char sqlite_header[sqlite_header_size] = "SQLite format 3";
    static const unsigned char truncated_bytes[] = "truncated mylite preamble";

    char path[test_path_capacity];
    unsigned char invalid_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char readback[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char plain_header[sqlite_header_size];
    mylite_db *database = NULL;
    long truncated_size = 0;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "invalid") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(invalid_preamble);
    invalid_preamble[MYLITE_FILE_RESERVED_OFFSET] = 1U;
    failures += write_file_bytes(path, invalid_preamble, sizeof(invalid_preamble));
    failures += expect_int(mylite_open(path, &database), MYLITE_ERROR, "reject invalid preamble");
    failures += expect_true(database == NULL, "invalid preamble leaves output null");
    failures += read_file_at(path, 0L, readback, sizeof(readback));
    failures +=
        expect_bytes(readback, invalid_preamble, sizeof(invalid_preamble), "invalid unchanged");
    remove_related_files(path);

    if (make_test_path(path, sizeof(path), "truncated") != 0) {
        return failures + 1;
    }
    remove_related_files(path);
    failures += write_file_bytes(path, truncated_bytes, sizeof(truncated_bytes));
    failures += expect_int(mylite_open(path, &database), MYLITE_ERROR, "reject truncated file");
    failures += expect_true(database == NULL, "truncated preamble leaves output null");
    failures += file_size(path, &truncated_size);
    failures +=
        expect_long(truncated_size, (long)sizeof(truncated_bytes), "truncated file unchanged");
    remove_related_files(path);

    if (make_test_path(path, sizeof(path), "plain") != 0) {
        return failures + 1;
    }
    remove_related_files(path);
    failures += create_plain_sqlite_database(path);
    failures += read_file_at(path, 0L, plain_header, sizeof(plain_header));
    failures += expect_bytes(plain_header, sqlite_header, sizeof(sqlite_header), "plain SQLite");
    failures += expect_int(mylite_open(path, &database), MYLITE_ERROR, "reject plain SQLite");
    failures += expect_true(database == NULL, "plain SQLite leaves output null");
    failures += read_file_at(path, 0L, plain_header, sizeof(plain_header));
    failures +=
        expect_bytes(plain_header, sqlite_header, sizeof(sqlite_header), "plain SQLite unchanged");
    remove_related_files(path);

    return failures;
}

static int test_independent_file_backed_handles_and_bootstrap_state(void) {
    enum {
        table_missing = 0,
        table_present = 1,
    };

    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    sqlite3 *first_sqlite = NULL;
    sqlite3 *second_sqlite = NULL;
    const struct mylite_sqlite_bootstrap_state *first_state = NULL;
    const struct mylite_sqlite_bootstrap_state *second_state = NULL;
    int first_has_table = table_missing;
    int second_has_table = table_missing;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first file handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second file handle");

    first_sqlite = mylite_connection_sqlite_for_test(first);
    second_sqlite = mylite_connection_sqlite_for_test(second);
    first_state = mylite_connection_sqlite_bootstrap_state_for_test(first);
    second_state = mylite_connection_sqlite_bootstrap_state_for_test(second);

    failures += expect_true(first_sqlite != NULL, "first file SQLite exists");
    failures += expect_true(second_sqlite != NULL, "second file SQLite exists");
    failures += expect_true(first_sqlite != second_sqlite, "file SQLite handles are distinct");
    failures += expect_true(first_state != NULL, "first bootstrap state exists");
    failures += expect_true(second_state != NULL, "second bootstrap state exists");
    if (first_state != NULL) {
        failures += expect_bool(first_state->initialized, true, "first bootstrap initialized");
        failures +=
            expect_bool(first_state->trusted_schema_is_enabled, false, "first trusted schema");
        failures += expect_bool(
            first_state->foreign_key_enforcement_is_enabled,
            false,
            "first foreign keys"
        );
        failures += expect_bool(
            first_state->foreign_key_policy_is_placeholder,
            true,
            "first foreign-key placeholder"
        );
    }
    if (second_state != NULL) {
        failures += expect_bool(second_state->initialized, true, "second bootstrap initialized");
        failures +=
            expect_bool(second_state->trusted_schema_is_enabled, false, "second trusted schema");
        failures += expect_bool(
            second_state->foreign_key_enforcement_is_enabled,
            false,
            "second foreign keys"
        );
    }

    if (first_sqlite != NULL && second_sqlite != NULL) {
        failures += execute_sql(first_sqlite, "CREATE TABLE file_marker(value INTEGER)");
        failures += table_exists(first_sqlite, "file_marker", &first_has_table);
        failures += table_exists(second_sqlite, "file_marker", &second_has_table);
    }
    failures += expect_int(first_has_table, table_present, "first file has marker table");
    failures += expect_int(second_has_table, table_missing, "second file lacks marker table");

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);

    return failures;
}

static int test_reopens_legacy_version_one_file(void) {
    static const unsigned char legacy_version_and_state[] = {
        0U,
        MYLITE_FILE_LEGACY_FORMAT_VERSION,
        0U,
    };

    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int stored_value = 0;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "legacy_v1") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "create legacy source file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += execute_sql(
            sqlite,
            "CREATE TABLE legacy_marker(value INTEGER);"
            "INSERT INTO legacy_marker(value) VALUES (73)"
        );
    }
    mylite_close(database);
    database = NULL;

    failures += write_file_at(
        path,
        MYLITE_FILE_FORMAT_VERSION_OFFSET,
        legacy_version_and_state,
        sizeof(legacy_version_and_state)
    );
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen legacy v1 file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += query_single_int(sqlite, "SELECT value FROM legacy_marker", &stored_value);
    }
    failures += expect_int(stored_value, reopen_marker_value, "legacy v1 payload value");

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_rejects_incomplete_lifecycle_files(void) {
    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char state = MYLITE_FILE_LIFECYCLE_INITIALIZING;
    char path[test_path_capacity];
    mylite_db *database = NULL;
    long stored_size = 0;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "empty_existing") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += write_file_bytes(path, "", 0U);
    failures +=
        expect_int(mylite_open(path, &database), MYLITE_ERROR, "reject empty existing file");
    failures += expect_true(database == NULL, "empty existing file leaves output null");
    remove_related_files(path);

    if (make_test_path(path, sizeof(path), "preamble_only") != 0) {
        return failures + 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(preamble);
    failures += write_file_bytes(path, preamble, sizeof(preamble));
    failures += expect_int(mylite_open(path, &database), MYLITE_ERROR, "reject preamble-only file");
    failures += expect_true(database == NULL, "preamble-only file leaves output null");
    failures += file_size(path, &stored_size);
    failures +=
        expect_long(stored_size, MYLITE_FILE_PREAMBLE_SIZE, "preamble-only file remains unchanged");
    remove_related_files(path);

    if (make_test_path(path, sizeof(path), "lifecycle") != 0) {
        return failures + 1;
    }
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "create lifecycle file");
    mylite_close(database);
    database = NULL;

    failures += write_file_at(path, MYLITE_FILE_LIFECYCLE_STATE_OFFSET, &state, sizeof(state));
    failures += expect_int(mylite_open(path, &database), MYLITE_ERROR, "reject initializing file");
    failures += expect_true(database == NULL, "initializing file leaves output null");
    state = MYLITE_FILE_LIFECYCLE_RECOVERY_REQUIRED;
    failures += write_file_at(path, MYLITE_FILE_LIFECYCLE_STATE_OFFSET, &state, sizeof(state));
    failures +=
        expect_int(mylite_open(path, &database), MYLITE_ERROR, "reject recovery-required file");
    failures += expect_true(database == NULL, "recovery-required file leaves output null");

    remove_related_files(path);
    return failures;
}

static int test_rejects_second_opener_during_initialization(void) {
    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE];
    char path[test_path_capacity];
    mylite_db *second = NULL;
    sqlite3 *initializing = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "concurrent_initialization") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(
        mylite_storage_open_sqlite_payload(path, &initializing),
        MYLITE_OK,
        "open initializing payload owner"
    );
    failures +=
        expect_int(mylite_open(path, &second), MYLITE_ERROR, "reject second initialization opener");
    failures += expect_true(second == NULL, "second initialization opener stays null");
    mylite_storage_abort_sqlite_initialization(initializing);
    failures += expect_int(sqlite3_close(initializing), SQLITE_OK, "close initializing owner");
    failures += read_file_at(path, 0L, preamble, sizeof(preamble));
    failures += expect_int(
        (int)mylite_file_preamble_lifecycle_state(preamble),
        MYLITE_FILE_LIFECYCLE_RECOVERY_REQUIRED,
        "aborted owner marks recovery required"
    );

    remove_related_files(path);
    return failures;
}

static int test_rejects_concurrent_process_opener(const char *executable_path) {
    char path[test_path_capacity];
    char ready_path[test_path_capacity];
    char release_path[test_path_capacity];
    mylite_db *second = NULL;
    int failures = 0;
    int written = 0;
#ifdef _WIN32
    intptr_t child = -1;
    int child_status = -1;
#else
    pid_t child = -1;
    int child_status = 0;

    (void)executable_path;
#endif

    if (make_test_path(path, sizeof(path), "concurrent_process_initialization") != 0) {
        return 1;
    }
    written = snprintf(ready_path, sizeof(ready_path), "%s-ready", path);
    if (written < 0 || (size_t)written >= sizeof(ready_path)) {
        return 1;
    }
    written = snprintf(release_path, sizeof(release_path), "%s-release", path);
    if (written < 0 || (size_t)written >= sizeof(release_path)) {
        return 1;
    }
    remove_related_files(path);
    (void)remove(ready_path);
    (void)remove(release_path);

#ifdef _WIN32
    child = _spawnl(
        _P_NOWAIT,
        executable_path,
        executable_path,
        "--initialization-child",
        path,
        ready_path,
        release_path,
        NULL
    );
#else
    child = fork();
    if (child == 0) {
        _exit(initialization_child_main(path, ready_path, release_path));
    }
#endif
    if (child == -1) {
        fprintf(stderr, "spawn concurrent initialization owner failed\n");
        failures += 1;
    } else {
        failures += wait_for_path(ready_path);
        if (failures == 0) {
            failures += expect_int(
                mylite_open(path, &second),
                MYLITE_ERROR,
                "reject concurrent process initialization opener"
            );
            failures +=
                expect_true(second == NULL, "concurrent process initialization opener stays null");
        }
        failures += write_file_bytes(release_path, "", 0U);
#ifdef _WIN32
        failures += expect_true(
            _cwait(&child_status, child, 0) != -1,
            "wait for concurrent initialization owner"
        );
        failures += expect_int(child_status, 0, "concurrent initialization owner status");
#else
        failures += expect_true(
            waitpid(child, &child_status, 0) == child,
            "wait for concurrent initialization owner"
        );
        if (WIFEXITED(child_status)) {
            failures +=
                expect_int(WEXITSTATUS(child_status), 0, "concurrent initialization owner status");
        } else {
            failures += 1;
        }
#endif
    }

    (void)remove(ready_path);
    (void)remove(release_path);
    remove_related_files(path);
    return failures;
}

static int test_vfs_fault_injection(void) {
    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE];
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    sqlite3_file *file = NULL;
    sqlite3_int64 logical_size = 0;
    int close_rc = SQLITE_OK;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "vfs_faults") != 0) {
        return 1;
    }
    remove_related_files(path);

    mylite_storage_vfs_test_set_fault(MYLITE_STORAGE_VFS_FAULT_CREATE, 1U);
    failures += expect_int(mylite_open(path, &database), MYLITE_ERROR, "inject create failure");
    failures += expect_true(database == NULL, "create failure leaves output null");
    failures +=
        expect_true(mylite_storage_vfs_test_fault_was_triggered(), "create failpoint triggered");
    failures += expect_int(path_exists(path), 0, "create failure leaves no file");
    mylite_storage_vfs_test_clear_fault();

    mylite_storage_vfs_test_set_fault(MYLITE_STORAGE_VFS_FAULT_WRITE, 1U);
    failures +=
        expect_int(mylite_open(path, &database), MYLITE_ERROR, "inject preamble write failure");
    failures += expect_true(database == NULL, "write failure leaves output null");
    failures +=
        expect_true(mylite_storage_vfs_test_fault_was_triggered(), "write failpoint triggered");
    mylite_storage_vfs_test_clear_fault();
    failures += read_file_at(path, 0L, preamble, sizeof(preamble));
    failures += expect_int(
        (int)mylite_file_preamble_lifecycle_state(preamble),
        MYLITE_FILE_LIFECYCLE_RECOVERY_REQUIRED,
        "write failure marks recovery required"
    );
    remove_related_files(path);

    mylite_storage_vfs_test_set_fault(MYLITE_STORAGE_VFS_FAULT_SYNC, 1U);
    failures +=
        expect_int(mylite_open(path, &database), MYLITE_ERROR, "inject preamble sync failure");
    failures += expect_true(database == NULL, "sync failure leaves output null");
    failures +=
        expect_true(mylite_storage_vfs_test_fault_was_triggered(), "sync failpoint triggered");
    mylite_storage_vfs_test_clear_fault();
    failures += read_file_at(path, 0L, preamble, sizeof(preamble));
    failures += expect_int(
        (int)mylite_file_preamble_lifecycle_state(preamble),
        MYLITE_FILE_LIFECYCLE_RECOVERY_REQUIRED,
        "sync failure marks recovery required"
    );
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "create VFS fault file");
    mylite_close(database);
    database = NULL;

    mylite_storage_vfs_test_set_fault(MYLITE_STORAGE_VFS_FAULT_OPEN, 1U);
    failures +=
        expect_int(mylite_open(path, &database), MYLITE_ERROR, "inject existing open failure");
    failures += expect_true(database == NULL, "open failure leaves output null");
    failures +=
        expect_true(mylite_storage_vfs_test_fault_was_triggered(), "open failpoint triggered");
    mylite_storage_vfs_test_clear_fault();

    failures += expect_int(
        mylite_storage_open_sqlite_payload(path, &sqlite),
        MYLITE_OK,
        "open payload for direct VFS faults"
    );
    if (sqlite != NULL) {
        failures += expect_int(
            sqlite3_file_control(sqlite, "main", SQLITE_FCNTL_FILE_POINTER, &file),
            SQLITE_OK,
            "load direct VFS file"
        );
    }
    if (file != NULL) {
        failures +=
            expect_int(file->pMethods->xFileSize(file, &logical_size), SQLITE_OK, "VFS size");

        mylite_storage_vfs_test_set_fault(MYLITE_STORAGE_VFS_FAULT_WRITE, 1U);
        failures += expect_int(
            file->pMethods->xWrite(file, preamble, 1, 0),
            SQLITE_IOERR_WRITE,
            "inject VFS write failure"
        );
        failures += expect_true(
            mylite_storage_vfs_test_fault_was_triggered(),
            "direct write failpoint triggered"
        );

        mylite_storage_vfs_test_set_fault(MYLITE_STORAGE_VFS_FAULT_SYNC, 1U);
        failures += expect_int(
            file->pMethods->xSync(file, SQLITE_SYNC_FULL),
            SQLITE_IOERR_FSYNC,
            "inject VFS sync failure"
        );
        failures += expect_true(
            mylite_storage_vfs_test_fault_was_triggered(),
            "direct sync failpoint triggered"
        );

        mylite_storage_vfs_test_set_fault(MYLITE_STORAGE_VFS_FAULT_TRUNCATE, 1U);
        failures += expect_int(
            file->pMethods->xTruncate(file, logical_size),
            SQLITE_IOERR_TRUNCATE,
            "inject VFS truncate failure"
        );
        failures += expect_true(
            mylite_storage_vfs_test_fault_was_triggered(),
            "truncate failpoint triggered"
        );
    }

    mylite_storage_vfs_test_set_fault(MYLITE_STORAGE_VFS_FAULT_CLOSE, 1U);
    close_rc = sqlite3_close(sqlite);
    sqlite = NULL;
    failures +=
        expect_true(mylite_storage_vfs_test_fault_was_triggered(), "close failpoint triggered");
    failures += expect_true(
        close_rc == SQLITE_OK || close_rc == SQLITE_IOERR_CLOSE,
        "close injection returns a documented SQLite close status"
    );
    mylite_storage_vfs_test_clear_fault();

    {
        char delete_path[test_path_capacity];
        sqlite3_vfs *vfs = sqlite3_vfs_find(mylite_storage_vfs_name());
        int written = snprintf(delete_path, sizeof(delete_path), "%s-delete", path);

        if (written < 0 || (size_t)written >= sizeof(delete_path)) {
            failures += 1;
        } else {
            failures += write_file_bytes(delete_path, "delete", sizeof("delete"));
            mylite_storage_vfs_test_set_fault(MYLITE_STORAGE_VFS_FAULT_DELETE, 1U);
            failures += expect_int(
                vfs->xDelete(vfs, delete_path, 0),
                SQLITE_IOERR_DELETE,
                "inject VFS delete failure"
            );
            failures += expect_true(
                mylite_storage_vfs_test_fault_was_triggered(),
                "delete failpoint triggered"
            );
            failures += expect_int(path_exists(delete_path), 1, "failed delete preserves file");
            mylite_storage_vfs_test_clear_fault();
            (void)remove(delete_path);
        }
    }

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen after VFS faults");
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_lock_byte_gap_mapping(void) {
    enum {
        crossing_size = 4,
        maximum_page_size = 65536,
        atomic_capabilities = SQLITE_IOCAP_ATOMIC | SQLITE_IOCAP_ATOMIC512 | SQLITE_IOCAP_ATOMIC1K |
                              SQLITE_IOCAP_ATOMIC2K | SQLITE_IOCAP_ATOMIC4K |
                              SQLITE_IOCAP_ATOMIC8K | SQLITE_IOCAP_ATOMIC16K |
                              SQLITE_IOCAP_ATOMIC32K | SQLITE_IOCAP_ATOMIC64K |
                              SQLITE_IOCAP_BATCH_ATOMIC,
    };

    static const unsigned char crossing_bytes[crossing_size] = {0x31U, 0x32U, 0x33U, 0x34U};
    static const int page_sizes[] = {512, 1024, 2048, 4096, 8192, 16384, 32768, 65536};
    static const unsigned char zeroes[MYLITE_FILE_SQLITE_PAYLOAD_OFFSET];

    static unsigned char page[maximum_page_size];
    static unsigned char page_readback[maximum_page_size];
    unsigned char gap[MYLITE_FILE_SQLITE_PAYLOAD_OFFSET];
    unsigned char readback[crossing_size];
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    sqlite3_file *main_file = NULL;
    sqlite3_int64 logical_size = 0;
    sqlite3_int64 mmap_size = 1024 * 1024;
    long physical_size = 0;
    int chunk_size = 1024 * 1024;
    int characteristics = 0;
    int failures = 0;
    size_t page_index = 0U;

    if (make_test_path(path, sizeof(path), "lock_gap") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "create lock-gap file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += expect_int(
            sqlite3_file_control(sqlite, "main", SQLITE_FCNTL_FILE_POINTER, &main_file),
            SQLITE_OK,
            "get lock-gap main file"
        );
    }
    if (main_file != NULL) {
        failures += expect_int(
            main_file->pMethods->xFileControl(main_file, SQLITE_FCNTL_CHUNK_SIZE, &chunk_size),
            SQLITE_OK,
            "disable incompatible physical chunk sizing"
        );
        for (page_index = 0U; page_index < sizeof(page_sizes) / sizeof(page_sizes[0]);
             ++page_index) {
            int page_size = page_sizes[page_index];
            int byte_index = 0;

            for (byte_index = 0; byte_index < page_size; ++byte_index) {
                page[byte_index] = (unsigned char)(byte_index + page_size);
            }
            failures += expect_int(
                main_file->pMethods->xWrite(
                    main_file,
                    page,
                    page_size,
                    MYLITE_FILE_LOCK_GAP_LOGICAL_OFFSET - (page_size / 2)
                ),
                SQLITE_OK,
                "write supported page size across logical lock split"
            );
            failures += expect_int(
                main_file->pMethods->xRead(
                    main_file,
                    page_readback,
                    page_size,
                    MYLITE_FILE_LOCK_GAP_LOGICAL_OFFSET - (page_size / 2)
                ),
                SQLITE_OK,
                "read supported page size across logical lock split"
            );
            failures += expect_bytes(
                page_readback,
                page,
                (size_t)page_size,
                "supported page-size logical readback"
            );
        }
        failures += expect_int(
            main_file->pMethods->xWrite(
                main_file,
                crossing_bytes,
                crossing_size,
                MYLITE_FILE_LOCK_GAP_LOGICAL_OFFSET - 2
            ),
            SQLITE_OK,
            "write across logical lock split"
        );
        failures += expect_int(
            main_file->pMethods->xRead(
                main_file,
                readback,
                crossing_size,
                MYLITE_FILE_LOCK_GAP_LOGICAL_OFFSET - 2
            ),
            SQLITE_OK,
            "read across logical lock split"
        );
        failures += expect_bytes(
            readback,
            crossing_bytes,
            sizeof(crossing_bytes),
            "crossing logical readback"
        );
        failures += expect_int(
            main_file->pMethods->xFileSize(main_file, &logical_size),
            SQLITE_OK,
            "read sparse logical size"
        );
        failures += expect_int64(
            logical_size,
            MYLITE_FILE_LOCK_GAP_LOGICAL_OFFSET + (maximum_page_size / 2),
            "sparse logical size"
        );
        failures += expect_int(
            main_file->pMethods->xFileControl(main_file, SQLITE_FCNTL_MMAP_SIZE, &mmap_size),
            SQLITE_OK,
            "disable mapped I/O"
        );
        failures += expect_int64(mmap_size, 0, "mapped I/O size remains zero");
        characteristics = main_file->pMethods->xDeviceCharacteristics(main_file);
        failures += expect_int(
            characteristics & atomic_capabilities,
            0,
            "shifted VFS clears atomic-write capabilities"
        );
        failures += expect_true(
            main_file->pMethods->xSectorSize(main_file) > 0 &&
                MYLITE_FILE_SQLITE_PAYLOAD_OFFSET % main_file->pMethods->xSectorSize(main_file) ==
                    0,
            "shifted VFS sector size preserves physical alignment"
        );
    }
    mylite_close(database);

    failures += file_size(path, &physical_size);
    failures += expect_long(
        physical_size,
        MYLITE_FILE_PHYSICAL_LOCK_BYTE + MYLITE_FILE_SQLITE_PAYLOAD_OFFSET +
            (maximum_page_size / 2),
        "sparse physical size includes lock gap"
    );
    failures += read_file_at(path, MYLITE_FILE_PHYSICAL_LOCK_BYTE - 2L, readback, 2U);
    failures += expect_bytes(readback, crossing_bytes, 2U, "bytes before physical lock gap");
    failures += read_file_at(path, MYLITE_FILE_PHYSICAL_LOCK_BYTE, gap, sizeof(gap));
    failures += expect_bytes(gap, zeroes, sizeof(gap), "physical lock gap remains empty");
    failures += read_file_at(
        path,
        MYLITE_FILE_PHYSICAL_LOCK_BYTE + MYLITE_FILE_SQLITE_PAYLOAD_OFFSET,
        readback,
        2U
    );
    failures += expect_bytes(&readback[0], &crossing_bytes[2], 2U, "bytes after physical lock gap");

    remove_related_files(path);
    return failures;
}

static int test_legacy_lock_boundary_containment(void) {
    static const unsigned char version_two[] = {0U, MYLITE_FILE_LIFECYCLE_FORMAT_VERSION};
    static const unsigned char marker = 0x5aU;

    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    sqlite3_file *main_file = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "legacy_lock_limit") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "create legacy-limit file");
    mylite_close(database);
    database = NULL;
    failures +=
        write_file_at(path, MYLITE_FILE_FORMAT_VERSION_OFFSET, version_two, sizeof(version_two));

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open version-two file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += expect_int(
            sqlite3_file_control(sqlite, "main", SQLITE_FCNTL_FILE_POINTER, &main_file),
            SQLITE_OK,
            "get legacy main file"
        );
    }
    if (main_file != NULL) {
        failures += expect_int(
            main_file->pMethods->xWrite(main_file, &marker, 1, MYLITE_FILE_LEGACY_MAX_LOGICAL_SIZE),
            SQLITE_FULL,
            "reject legacy write into physical lock range"
        );
        failures += expect_int(
            main_file->pMethods->xTruncate(main_file, MYLITE_FILE_LEGACY_MAX_LOGICAL_SIZE + 1),
            SQLITE_FULL,
            "reject legacy truncate into physical lock range"
        );
    }
    mylite_close(database);
    database = NULL;

    failures += write_file_at(path, MYLITE_FILE_PHYSICAL_LOCK_BYTE, &marker, sizeof(marker));
    failures +=
        expect_int(mylite_open(path, &database), MYLITE_ERROR, "reject oversized version-two file");
    failures += expect_true(database == NULL, "oversized version-two output remains null");

    remove_related_files(path);
    return failures;
}

static int test_journal_mode_policy(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "journal_policy") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open journal-policy file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += query_single_text_equals(sqlite, "PRAGMA journal_mode", "delete");
        failures += query_single_text_equals(sqlite, "PRAGMA journal_mode=WAL", "delete");
        failures += query_single_text_equals(sqlite, "PRAGMA journal_mode", "delete");
    }

    mylite_close(database);
    failures += expect_int(file_exists_with_suffix(path, "-wal"), 0, "WAL file is not created");
    failures +=
        expect_int(file_exists_with_suffix(path, "-shm"), 0, "shared-memory file is not created");

    remove_related_files(path);
    return failures;
}

#ifndef _WIN32
static int test_process_death_leaves_initializing_file_rejected(void) {
    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE];
    char path[test_path_capacity];
    mylite_db *database = NULL;
    pid_t child = 0;
    int child_status = 0;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "initialization_process_death") != 0) {
        return 1;
    }
    remove_related_files(path);

    child = fork();
    if (child == 0) {
        sqlite3 *initializing = NULL;
        int rc = mylite_storage_open_sqlite_payload(path, &initializing);

        _exit(rc == MYLITE_OK && initializing != NULL ? 0 : 1);
    }
    if (child < 0) {
        fprintf(stderr, "fork initialization owner failed\n");
        remove_related_files(path);
        return 1;
    }
    if (waitpid(child, &child_status, 0) != child) {
        fprintf(stderr, "wait for initialization owner failed\n");
        failures += 1;
    } else {
        failures += expect_true(WIFEXITED(child_status), "initialization owner exited");
        if (WIFEXITED(child_status)) {
            failures +=
                expect_int(WEXITSTATUS(child_status), 0, "initialization owner opened payload");
        }
    }

    failures += read_file_at(path, 0L, preamble, sizeof(preamble));
    failures += expect_int(
        (int)mylite_file_preamble_lifecycle_state(preamble),
        MYLITE_FILE_LIFECYCLE_INITIALIZING,
        "process death preserves initializing state"
    );
    failures += expect_int(
        mylite_open(path, &database),
        MYLITE_ERROR,
        "reject initializer process-death file"
    );
    failures += expect_true(database == NULL, "process-death file leaves output null");

    remove_related_files(path);
    return failures;
}

static int test_abort_marks_opened_identity_recovery_required(void) {
    static const unsigned char replacement[] = "replacement database path";

    unsigned char owned_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char replacement_readback[sizeof(replacement)];
    char owned_path[test_path_capacity];
    char path[test_path_capacity];
    sqlite3 *initializing = NULL;
    int failures = 0;
    int written = 0;

    if (make_test_path(path, sizeof(path), "identity_replacement") != 0) {
        return 1;
    }
    written = snprintf(owned_path, sizeof(owned_path), "%s-owned", path);
    if (written < 0 || (size_t)written >= sizeof(owned_path)) {
        return 1;
    }
    remove_related_files(path);
    remove_related_files(owned_path);

    failures += expect_int(
        mylite_storage_open_sqlite_payload(path, &initializing),
        MYLITE_OK,
        "open identity owner"
    );
    failures += expect_int(rename(path, owned_path), 0, "rename opened identity");
    failures += write_file_bytes(path, replacement, sizeof(replacement));
    mylite_storage_abort_sqlite_initialization(initializing);
    failures += expect_int(sqlite3_close(initializing), SQLITE_OK, "close renamed identity owner");

    failures += read_file_at(path, 0L, replacement_readback, sizeof(replacement_readback));
    failures += expect_bytes(
        replacement_readback,
        replacement,
        sizeof(replacement),
        "replacement path remains unchanged"
    );
    failures += read_file_at(owned_path, 0L, owned_preamble, sizeof(owned_preamble));
    failures += expect_int(
        (int)mylite_file_preamble_lifecycle_state(owned_preamble),
        MYLITE_FILE_LIFECYCLE_RECOVERY_REQUIRED,
        "opened identity receives recovery state"
    );

    remove_related_files(path);
    remove_related_files(owned_path);
    return failures;
}
#endif

static int test_symlink_failure_preserves_path_identity(void) {
    static const unsigned char target_contents[] = "not a MyLite database";

    unsigned char readback[sizeof(target_contents)];
    char link_path[test_path_capacity];
    char target_path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;
    int symlink_rc = 0;
    int written = 0;

    if (make_test_path(link_path, sizeof(link_path), "symlink_identity") != 0) {
        return 1;
    }
    written = snprintf(target_path, sizeof(target_path), "%s-target", link_path);
    if (written < 0 || (size_t)written >= sizeof(target_path)) {
        return 1;
    }
    remove_related_files(link_path);
    remove_related_files(target_path);
    failures += write_file_bytes(target_path, target_contents, sizeof(target_contents));
    symlink_rc = create_file_symlink(target_path, link_path);
    if (symlink_rc == 2) {
        fprintf(stderr, "SKIP: operating system denied test symlink creation\n");
        remove_related_files(target_path);
        return failures;
    }
    failures += expect_int(symlink_rc, 0, "create database symlink");
    if (symlink_rc != 0) {
        remove_related_files(link_path);
        remove_related_files(target_path);
        return failures;
    }

    failures +=
        expect_int(mylite_open(link_path, &database), MYLITE_ERROR, "reject symlink target");
    failures += expect_true(database == NULL, "symlink failure leaves output null");
    failures += expect_int(path_is_symlink(link_path), 1, "failed open preserves symlink");
    failures += read_file_at(target_path, 0L, readback, sizeof(readback));
    failures += expect_bytes(
        readback,
        target_contents,
        sizeof(target_contents),
        "failed symlink open preserves target"
    );

    remove_related_files(link_path);
    remove_related_files(target_path);
    return failures;
}

static int create_file_symlink(const char *target_path, const char *link_path) {
#ifdef _WIN32
    enum { allow_unprivileged_create = 0x2 };

    if (CreateSymbolicLinkA(link_path, target_path, allow_unprivileged_create) != 0) {
        return 0;
    }
    return GetLastError() == ERROR_PRIVILEGE_NOT_HELD ? 2 : 1;
#else
    return symlink(target_path, link_path);
#endif
}

static int path_is_symlink(const char *path) {
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path);

    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    struct stat status;

    return lstat(path, &status) == 0 && S_ISLNK(status.st_mode);
#endif
}

#ifndef _WIN32

static int test_hot_journal_recovery_after_process_death(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    pid_t child = 0;
    int child_status = 0;
    int changed_rows = -1;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "hot_journal") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "create recovery file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += execute_sql(
            sqlite,
            "CREATE TABLE recovery_rows(id INTEGER PRIMARY KEY, value BLOB);"
            "WITH RECURSIVE n(value) AS ("
            "SELECT 1 UNION ALL SELECT value + 1 FROM n WHERE value < 64"
            ") INSERT INTO recovery_rows SELECT value, zeroblob(4096) FROM n;"
        );
    }
    mylite_close(database);
    database = NULL;

    child = fork();
    if (child == 0) {
        mylite_db *child_database = NULL;
        sqlite3 *child_sqlite = NULL;
        int rc = mylite_open(path, &child_database);

        if (rc == MYLITE_OK) {
            child_sqlite = mylite_connection_sqlite_for_test(child_database);
            rc = sqlite3_exec(
                child_sqlite,
                "PRAGMA cache_size=1;"
                "PRAGMA cache_spill=ON;"
                "BEGIN IMMEDIATE;"
                "UPDATE recovery_rows SET value = randomblob(4096);",
                NULL,
                NULL,
                NULL
            );
        }
        _exit(rc == SQLITE_OK ? 0 : 1);
    }
    if (child < 0) {
        fprintf(stderr, "fork recovery writer failed\n");
        remove_related_files(path);
        return failures + 1;
    }
    if (waitpid(child, &child_status, 0) != child) {
        fprintf(stderr, "wait for recovery writer failed\n");
        failures += 1;
    } else {
        failures += expect_true(WIFEXITED(child_status), "recovery writer exited");
        if (WIFEXITED(child_status)) {
            failures += expect_int(WEXITSTATUS(child_status), 0, "recovery writer updated rows");
        }
    }
    failures += expect_int(
        file_exists_with_suffix(path, "-journal"),
        1,
        "interrupted transaction leaves rollback journal"
    );

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "recover hot journal");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += query_single_int(
            sqlite,
            "SELECT count(*) FROM recovery_rows WHERE value != zeroblob(4096)",
            &changed_rows
        );
    }
    failures += expect_int(changed_rows, 0, "hot-journal rollback restores committed rows");
    mylite_close(database);
    failures += expect_int(
        file_exists_with_suffix(path, "-journal"),
        0,
        "hot rollback journal removed after recovery"
    );

    remove_related_files(path);
    return failures;
}
#endif

static int initialization_child_main(
    const char *path,
    const char *ready_path,
    const char *release_path
) {
    sqlite3 *initializing = NULL;
    int rc = mylite_storage_open_sqlite_payload(path, &initializing);

    if (rc != MYLITE_OK || initializing == NULL) {
        return 1;
    }
    if (write_file_bytes(ready_path, "", 0U) != 0) {
        mylite_storage_abort_sqlite_initialization(initializing);
        (void)sqlite3_close(initializing);
        return 1;
    }
    if (wait_for_path(release_path) != 0) {
        mylite_storage_abort_sqlite_initialization(initializing);
        (void)sqlite3_close(initializing);
        return 1;
    }

    mylite_storage_abort_sqlite_initialization(initializing);
    return sqlite3_close(initializing) == SQLITE_OK ? 0 : 1;
}

static int wait_for_path(const char *path) {
    for (int attempt = 0; attempt < 500; ++attempt) {
        if (path_exists(path)) {
            return 0;
        }
        (void)sqlite3_sleep(10);
    }

    fprintf(stderr, "timed out waiting for path: %s\n", path);
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
        "%s/mylite_file_backed_open_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long for %s\n", name);
        return 1;
    }

    return 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return (int)getpid();
#endif
}

static void remove_related_files(const char *path) {
    (void)remove(path);
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }

    (void)remove(related_path);
}

static int write_file_bytes(const char *path, const void *bytes, size_t size) {
    FILE *file = fopen(path, "wb");
    int failures = 0;

    if (file == NULL) {
        fprintf(stderr, "open %s for writing failed\n", path);
        return 1;
    }

    if (fwrite(bytes, 1U, size, file) != size) {
        fprintf(stderr, "write %s failed\n", path);
        failures += 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "close %s after writing failed\n", path);
        failures += 1;
    }

    return failures;
}

static int write_file_at(const char *path, long offset, const void *bytes, size_t size) {
    FILE *file = fopen(path, "r+b");
    int failures = 0;

    if (file == NULL) {
        fprintf(stderr, "open %s for update failed\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "seek %s to %ld for update failed\n", path, offset);
        failures += 1;
    } else if (fwrite(bytes, 1U, size, file) != size) {
        fprintf(stderr, "update %zu bytes in %s at %ld failed\n", size, path, offset);
        failures += 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "close %s after update failed\n", path);
        failures += 1;
    }

    return failures;
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    int failures = 0;

    if (file == NULL) {
        fprintf(stderr, "open %s for reading failed\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "seek %s to %ld failed\n", path, offset);
        failures += 1;
    } else if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "read %zu bytes from %s at %ld failed\n", size, path, offset);
        failures += 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "close %s after reading failed\n", path);
        failures += 1;
    }

    return failures;
}

static int file_size(const char *path, long *out_size) {
    FILE *file = fopen(path, "rb");
    int failures = 0;

    *out_size = 0;
    if (file == NULL) {
        fprintf(stderr, "open %s for size failed\n", path);
        return 1;
    }
    if (fseek(file, 0L, SEEK_END) != 0) {
        fprintf(stderr, "seek %s to end failed\n", path);
        failures += 1;
    } else {
        *out_size = ftell(file);
        if (*out_size < 0L) {
            fprintf(stderr, "tell %s size failed\n", path);
            failures += 1;
        }
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "close %s after size failed\n", path);
        failures += 1;
    }

    return failures;
}

static int file_exists_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity];
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

static int create_plain_sqlite_database(const char *path) {
    sqlite3 *connection = NULL;
    int rc = sqlite3_open(path, &connection);
    int failures = 0;

    if (rc != SQLITE_OK) {
        fprintf(
            stderr,
            "open plain SQLite %s failed: %s\n",
            path,
            connection == NULL ? "out of memory" : sqlite3_errmsg(connection)
        );
        (void)sqlite3_close(connection);
        return 1;
    }

    failures += execute_sql(connection, "CREATE TABLE plain_marker(value INTEGER)");
    rc = sqlite3_close(connection);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "close plain SQLite %s failed: %d\n", path, rc);
        failures += 1;
    }

    return failures;
}

static int execute_sql(sqlite3 *connection, const char *sql) {
    char *message = NULL;
    int rc = sqlite3_exec(connection, sql, NULL, NULL, &message);

    if (rc != SQLITE_OK) {
        fprintf(
            stderr,
            "execute SQLite SQL \"%s\": error %d: %s\n",
            sql,
            rc,
            message == NULL ? "(no message)" : message
        );
        sqlite3_free(message);
        return 1;
    }

    return 0;
}

static int query_single_int(sqlite3 *connection, const char *sql, int *out_value) {
    enum { sqlite_use_nul_terminated_string = -1 };

    sqlite3_stmt *statement = NULL;
    int rc = SQLITE_OK;

    *out_value = 0;
    rc = sqlite3_prepare_v2(connection, sql, sqlite_use_nul_terminated_string, &statement, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "prepare SQLite SQL \"%s\": error %d\n", sql, rc);
        return 1;
    }

    rc = sqlite3_step(statement);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "step SQLite SQL \"%s\": error %d\n", sql, rc);
        (void)sqlite3_finalize(statement);
        return 1;
    }
    *out_value = sqlite3_column_int(statement, 0);

    rc = sqlite3_finalize(statement);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "finalize SQLite SQL \"%s\": error %d\n", sql, rc);
        return 1;
    }

    return 0;
}

static int query_single_text_equals(sqlite3 *connection, const char *sql, const char *expected) {
    enum { sqlite_use_nul_terminated_string = -1 };

    sqlite3_stmt *statement = NULL;
    const unsigned char *actual = NULL;
    int failures = 0;
    int rc =
        sqlite3_prepare_v2(connection, sql, sqlite_use_nul_terminated_string, &statement, NULL);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "prepare SQLite SQL \"%s\": error %d\n", sql, rc);
        return 1;
    }
    rc = sqlite3_step(statement);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "step SQLite SQL \"%s\": error %d\n", sql, rc);
        (void)sqlite3_finalize(statement);
        return 1;
    }
    actual = sqlite3_column_text(statement, 0);
    if (actual == NULL || strcmp((const char *)actual, expected) != 0) {
        fprintf(
            stderr,
            "SQLite SQL \"%s\": expected %s, got %s\n",
            sql,
            expected,
            actual == NULL ? "NULL" : (const char *)actual
        );
        failures += 1;
    }
    rc = sqlite3_finalize(statement);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "finalize SQLite SQL \"%s\": error %d\n", sql, rc);
        failures += 1;
    }

    return failures;
}

static int table_exists(sqlite3 *connection, const char *table_name, int *out_exists) {
    enum { sqlite_use_nul_terminated_string = -1 };

    static const char *sql =
        "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name = ?1";

    sqlite3_stmt *statement = NULL;
    int rc = SQLITE_OK;

    *out_exists = 0;
    rc = sqlite3_prepare_v2(connection, sql, sqlite_use_nul_terminated_string, &statement, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "prepare table existence query: SQLite error %d\n", rc);
        return 1;
    }

    rc = sqlite3_bind_text(
        statement,
        1,
        table_name,
        sqlite_use_nul_terminated_string,
        SQLITE_STATIC
    );
    if (rc != SQLITE_OK) {
        fprintf(stderr, "bind table name: SQLite error %d\n", rc);
        (void)sqlite3_finalize(statement);
        return 1;
    }

    rc = sqlite3_step(statement);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "step table existence query: SQLite error %d\n", rc);
        (void)sqlite3_finalize(statement);
        return 1;
    }

    *out_exists = sqlite3_column_int(statement, 0) > 0 ? 1 : 0;

    rc = sqlite3_finalize(statement);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "finalize table existence query: SQLite error %d\n", rc);
        return 1;
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

static int expect_long(long actual, long expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %ld, got %ld\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_int64(sqlite3_int64 actual, sqlite3_int64 expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %" PRId64 ", got %" PRId64 "\n",
            context,
            (int64_t)expected,
            (int64_t)actual
        );
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

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte sequence mismatch\n", context);
        return 1;
    }

    return 0;
}

static int expect_not_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) == 0) {
        fprintf(stderr, "%s: byte sequence unexpectedly matched\n", context);
        return 1;
    }

    return 0;
}
