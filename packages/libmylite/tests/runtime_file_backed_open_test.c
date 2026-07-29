#include "mylite_test_support.h"

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
    initialization_child_argument_count = 5,
    initialization_death_child_argument_count = 4,
    hot_journal_child_argument_count = 3,
    decimal_radix = 10,
    lock_gap_control_size = 1024 * 1024,
    path_wait_attempt_count = 500,
    path_wait_sleep_ms = 10,
    sqlite_synchronous_extra = 3,
};

struct initialization_child_paths {
    const char *database;
    const char *ready;
    const char *release;
};

struct child_process_paths {
    const char *executable;
    const char *database;
};

static int test_open_rejects_invalid_arguments(void);
static int test_sized_open_path_contract(void);
static int test_sized_open_rejection_has_no_vfs_side_effects(void);
static int test_create_new_file_with_preamble_and_shifted_payload(void);
static int test_reopen_existing_file_preserves_sqlite_payload(void);
static int test_rejects_invalid_truncated_and_plain_sqlite_files(void);
static int test_independent_file_backed_handles_and_bootstrap_state(void);
static int test_reopens_legacy_version_one_file(void);
static int test_recovers_incomplete_lifecycle_files(void);
static int test_rejects_second_opener_during_initialization(void);
static int test_rejects_concurrent_process_opener(const char *executable_path);
static int test_recovers_after_initialization_process_death(const char *executable_path);
static int test_vfs_fault_injection(void);
static int test_lock_byte_gap_mapping(void);
static int test_legacy_lock_boundary_containment(void);
static int test_journal_mode_policy(void);
static int test_hot_journal_recovery_after_process_death(const char *executable_path);
#ifndef _WIN32
static int test_abort_marks_opened_identity_recovery_required(void);
#endif
static int test_symlink_failure_preserves_path_identity(void);
static int create_file_symlink(const char *target_path, const char *link_path);
static int path_is_symlink(const char *path);
static int initialization_child_main(const struct initialization_child_paths *paths);
static int initialization_death_child_main(
    const char *path,
    enum mylite_file_initialization_test_event target_event
);
static void initialization_death_test_hook(
    enum mylite_file_initialization_test_event event,
    void *context
);
static int run_initialization_death_child(
    const struct child_process_paths *paths,
    enum mylite_file_initialization_test_event event
);
static int hot_journal_child_main(const char *path);
static int run_hot_journal_child(const struct child_process_paths *paths);
static int wait_for_path(const char *path);
static int path_exists(const char *path);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static void remove_path(const char *path);
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
static int expect_long(long actual, long expected, const char *context);
static int expect_int64(sqlite3_int64 actual, sqlite3_int64 expected, const char *context);
static int expect_bool(bool actual, bool expected, const char *context);
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

    if (argc == initialization_child_argument_count &&
        strcmp(argv[1], "--initialization-child") == 0) {
        return initialization_child_main(&(const struct initialization_child_paths){
            .database = argv[2],
            .ready = argv[3],
            .release = argv[4],
        });
    }
    if (argc == initialization_death_child_argument_count &&
        strcmp(argv[1], "--initialization-death-child") == 0) {
        long event = strtol(argv[3], NULL, decimal_radix);

        if (event < MYLITE_FILE_INITIALIZATION_PAYLOAD_OPENED ||
            event > MYLITE_FILE_INITIALIZATION_AFTER_LIFECYCLE_PUBLICATION) {
            return 1;
        }
        return initialization_death_child_main(
            argv[2],
            (enum mylite_file_initialization_test_event)event
        );
    }
    if (argc == hot_journal_child_argument_count && strcmp(argv[1], "--hot-journal-child") == 0) {
        return hot_journal_child_main(argv[2]);
    }

    failures += test_open_rejects_invalid_arguments();
    failures += test_sized_open_path_contract();
    failures += test_sized_open_rejection_has_no_vfs_side_effects();
    failures += test_create_new_file_with_preamble_and_shifted_payload();
    failures += test_reopen_existing_file_preserves_sqlite_payload();
    failures += test_rejects_invalid_truncated_and_plain_sqlite_files();
    failures += test_independent_file_backed_handles_and_bootstrap_state();
    failures += test_reopens_legacy_version_one_file();
    failures += test_recovers_incomplete_lifecycle_files();
    failures += test_rejects_second_opener_during_initialization();
    failures += test_rejects_concurrent_process_opener(argv[0]);
    failures += test_recovers_after_initialization_process_death(argv[0]);
    failures += test_vfs_fault_injection();
    failures += test_lock_byte_gap_mapping();
    failures += test_legacy_lock_boundary_containment();
    failures += test_journal_mode_policy();
    failures += test_hot_journal_recovery_after_process_death(argv[0]);
#ifndef _WIN32
    failures += test_abort_marks_opened_identity_recovery_required();
#endif
    failures += test_symlink_failure_preserves_path_identity();

    return failures == 0 ? 0 : 1;
}

static int test_open_rejects_invalid_arguments(void) {
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open(NULL, &database), MYLITE_MISUSE, "reject NULL path");
    failures += mylite_test_expect_true(database == NULL, "NULL path leaves output null");
    failures +=
        mylite_test_expect_int(mylite_open("", &database), MYLITE_MISUSE, "reject empty path");
    failures += mylite_test_expect_true(database == NULL, "empty path leaves output null");
    failures += mylite_test_expect_int(
        mylite_open("unused.mylite", NULL),
        MYLITE_MISUSE,
        "reject NULL output"
    );

    return failures;
}

static int test_sized_open_path_contract(void) {
    static const char nul_at_start[] = {'\0', 'x'};
    static const char nul_in_middle[] = {'x', '\0', 'y'};
    static const char memory_with_nul[] = {':', 'm', 'e', 'm', 'o', 'r', 'y', ':', '\0'};
    static const unsigned char sentinel[] = "sized-open-prefix-sentinel";

    char path[test_path_capacity];
    char path_span[test_path_capacity];
    char rejected_path[test_path_capacity * 2U];
    unsigned char readback[sizeof(sentinel)];
    struct mylite_open_diagnostic diagnostic;
    mylite_db *database = NULL;
    size_t path_size = 0U;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_open_with_size(NULL, 0U, &database),
        MYLITE_MISUSE,
        "sized open rejects NULL path"
    );
    failures += mylite_test_expect_true(database == NULL, "NULL sized path leaves output null");
    failures += mylite_test_expect_int(
        mylite_open_with_size("", 0U, &database),
        MYLITE_MISUSE,
        "sized open rejects empty span"
    );
    failures += mylite_test_expect_int(
        mylite_open_with_size("x", SIZE_MAX, &database),
        MYLITE_MISUSE,
        "sized open rejects unterminatable span"
    );
    failures += mylite_test_expect_int(
        mylite_open_with_size("unused", strlen("unused"), NULL),
        MYLITE_MISUSE,
        "sized open rejects NULL output"
    );

    failures += mylite_test_expect_int(
        mylite_open_with_size_and_diagnostic(
            nul_in_middle,
            sizeof(nul_in_middle),
            &database,
            &diagnostic
        ),
        MYLITE_MISUSE,
        "diagnostic sized open rejects embedded NUL"
    );
    failures +=
        mylite_test_expect_true(database == NULL, "diagnostic embedded NUL leaves output null");
    failures += mylite_test_expect_int(
        diagnostic.error_code,
        MYLITE_MISUSE,
        "diagnostic embedded NUL error code"
    );
    failures += mylite_test_expect_true(
        strcmp(diagnostic.sqlstate, "HY000") == 0,
        "diagnostic embedded NUL SQLSTATE"
    );
    failures += mylite_test_expect_true(
        strcmp(diagnostic.message, "invalid MyLite open arguments") == 0,
        "diagnostic embedded NUL message"
    );
    failures += mylite_test_expect_int(
        mylite_open_with_size(nul_at_start, sizeof(nul_at_start), &database),
        MYLITE_MISUSE,
        "sized open rejects leading NUL"
    );
    failures += mylite_test_expect_int(
        mylite_open_with_size(memory_with_nul, sizeof(memory_with_nul), &database),
        MYLITE_MISUSE,
        "sized open rejects NUL-suffixed memory token"
    );

    if (mylite_test_make_path(path, sizeof(path), "sized_nonterminated") != 0) {
        return failures + 1;
    }
    remove_related_files(path);
    path_size = strlen(path);
    memcpy(path_span, path, path_size);
    path_span[path_size] = 'X';
    failures += mylite_test_expect_int(
        mylite_open_with_size(path_span, path_size, &database),
        MYLITE_OK,
        "sized open accepts nonterminated span"
    );
    mylite_close(database);
    database = NULL;
    failures += expect_bool(path_exists(path) != 0, true, "nonterminated path creates exact file");
    remove_related_files(path);

    if (mylite_test_make_path(path, sizeof(path), "sized_\xC3\xA9") != 0) {
        return failures + 1;
    }
    remove_related_files(path);
    failures += mylite_test_expect_int(
        mylite_open_with_size(path, strlen(path), &database),
        MYLITE_OK,
        "sized open accepts non-ASCII path"
    );
    mylite_close(database);
    database = NULL;
    failures += expect_bool(path_exists(path) != 0, true, "non-ASCII path creates exact file");
    remove_related_files(path);

    if (mylite_test_make_path(path, sizeof(path), "sized_absent_prefix") != 0) {
        return failures + 1;
    }
    remove_related_files(path);
    path_size = strlen(path);
    memcpy(rejected_path, path, path_size);
    rejected_path[path_size] = '\0';
    memcpy(&rejected_path[path_size + 1U], ".bypass", strlen(".bypass"));
    failures += mylite_test_expect_int(
        mylite_open_with_size(rejected_path, path_size + 1U + strlen(".bypass"), &database),
        MYLITE_MISUSE,
        "sized open rejects absent authorized-prefix bypass"
    );
    failures += expect_bool(path_exists(path) != 0, false, "rejected prefix is not created");

    if (mylite_test_make_path(path, sizeof(path), "sized_existing_prefix") != 0) {
        return failures + 1;
    }
    remove_related_files(path);
    failures += write_file_bytes(path, sentinel, sizeof(sentinel));
    path_size = strlen(path);
    memcpy(rejected_path, path, path_size);
    rejected_path[path_size] = '\0';
    memcpy(&rejected_path[path_size + 1U], ".bypass", strlen(".bypass"));
    failures += mylite_test_expect_int(
        mylite_open_with_size(rejected_path, path_size + 1U + strlen(".bypass"), &database),
        MYLITE_MISUSE,
        "sized open rejects existing authorized-prefix bypass"
    );
    failures += read_file_at(path, 0L, readback, sizeof(readback));
    failures += expect_bytes(
        readback,
        sentinel,
        sizeof(sentinel),
        "rejected existing prefix remains unchanged"
    );
    remove_related_files(path);

    return failures;
}

static int test_sized_open_rejection_has_no_vfs_side_effects(void) {
    static const enum mylite_storage_vfs_fault_operation operations[] = {
        MYLITE_STORAGE_VFS_FAULT_CREATE,
        MYLITE_STORAGE_VFS_FAULT_OPEN,
        MYLITE_STORAGE_VFS_FAULT_TRUNCATE,
        MYLITE_STORAGE_VFS_FAULT_DELETE,
    };

    static const char invalid_path[] = {'n', 'o', '\0', 'p', 'a', 't', 'h'};

    mylite_db *database = NULL;
    int failures = 0;

    for (size_t index = 0U; index < sizeof(operations) / sizeof(operations[0]); ++index) {
        mylite_storage_vfs_test_set_fault(operations[index], 1U);
        failures += mylite_test_expect_int(
            mylite_open_with_size(invalid_path, sizeof(invalid_path), &database),
            MYLITE_MISUSE,
            "invalid sized path rejected before VFS"
        );
        failures +=
            expect_bool(mylite_storage_vfs_test_fault_was_triggered(), false, "VFS fault unused");
        failures += mylite_test_expect_size(
            mylite_storage_vfs_test_matching_call_count(),
            0U,
            "rejected path has zero matching VFS calls"
        );
        mylite_storage_vfs_test_clear_fault();
    }

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

    if (mylite_test_make_path(path, sizeof(path), "create") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open new file-backed handle"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += mylite_test_expect_true(sqlite != NULL, "file-backed SQLite connection exists");
    if (sqlite != NULL) {
        failures += execute_sql(
            sqlite,
            "CREATE TABLE shifted_payload(value INTEGER);"
            "INSERT INTO shifted_payload(value) VALUES (42)"
        );
    }
    mylite_close(database);

    failures += read_file_at(path, 0L, preamble, sizeof(preamble));
    failures += mylite_test_expect_int(
        mylite_file_preamble_validate(preamble),
        1,
        "created preamble validates"
    );
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

    if (mylite_test_make_path(path, sizeof(path), "reopen") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open file to populate");
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

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen existing file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += query_single_int(sqlite, "SELECT value FROM reopen_marker", &stored_value);
    }
    failures +=
        mylite_test_expect_int(stored_value, reopen_marker_value, "reopened payload preserves row");
    if (sqlite != NULL) {
        failures += execute_sql(sqlite, "UPDATE reopen_marker SET value = 74");
        failures += query_single_int(sqlite, "SELECT value FROM reopen_marker", &stored_value);
    }
    failures += mylite_test_expect_int(
        stored_value,
        updated_reopen_marker_value,
        "reopened payload remains writable"
    );

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

    if (mylite_test_make_path(path, sizeof(path), "invalid") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(invalid_preamble);
    invalid_preamble[MYLITE_FILE_RESERVED_OFFSET] = 1U;
    failures += write_file_bytes(path, invalid_preamble, sizeof(invalid_preamble));
    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_ERROR,
        "reject invalid preamble"
    );
    failures += mylite_test_expect_true(database == NULL, "invalid preamble leaves output null");
    failures += read_file_at(path, 0L, readback, sizeof(readback));
    failures +=
        expect_bytes(readback, invalid_preamble, sizeof(invalid_preamble), "invalid unchanged");
    remove_related_files(path);

    if (mylite_test_make_path(path, sizeof(path), "truncated") != 0) {
        return failures + 1;
    }
    remove_related_files(path);
    failures += write_file_bytes(path, truncated_bytes, sizeof(truncated_bytes));
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_ERROR, "reject truncated file");
    failures += mylite_test_expect_true(database == NULL, "truncated preamble leaves output null");
    failures += file_size(path, &truncated_size);
    failures +=
        expect_long(truncated_size, (long)sizeof(truncated_bytes), "truncated file unchanged");
    remove_related_files(path);

    if (mylite_test_make_path(path, sizeof(path), "plain") != 0) {
        return failures + 1;
    }
    remove_related_files(path);
    failures += create_plain_sqlite_database(path);
    failures += read_file_at(path, 0L, plain_header, sizeof(plain_header));
    failures += expect_bytes(plain_header, sqlite_header, sizeof(sqlite_header), "plain SQLite");
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_ERROR, "reject plain SQLite");
    failures += mylite_test_expect_true(database == NULL, "plain SQLite leaves output null");
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

    if (mylite_test_make_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        mylite_test_make_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += mylite_test_expect_int(
        mylite_open(first_path, &first),
        MYLITE_OK,
        "open first file handle"
    );
    failures += mylite_test_expect_int(
        mylite_open(second_path, &second),
        MYLITE_OK,
        "open second file handle"
    );

    first_sqlite = mylite_connection_sqlite_for_test(first);
    second_sqlite = mylite_connection_sqlite_for_test(second);
    first_state = mylite_connection_sqlite_bootstrap_state_for_test(first);
    second_state = mylite_connection_sqlite_bootstrap_state_for_test(second);

    failures += mylite_test_expect_true(first_sqlite != NULL, "first file SQLite exists");
    failures += mylite_test_expect_true(second_sqlite != NULL, "second file SQLite exists");
    failures +=
        mylite_test_expect_true(first_sqlite != second_sqlite, "file SQLite handles are distinct");
    failures += mylite_test_expect_true(first_state != NULL, "first bootstrap state exists");
    failures += mylite_test_expect_true(second_state != NULL, "second bootstrap state exists");
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
    failures +=
        mylite_test_expect_int(first_has_table, table_present, "first file has marker table");
    failures +=
        mylite_test_expect_int(second_has_table, table_missing, "second file lacks marker table");

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

    if (mylite_test_make_path(path, sizeof(path), "legacy_v1") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "create legacy source file"
    );
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
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen legacy v1 file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += query_single_int(sqlite, "SELECT value FROM legacy_marker", &stored_value);
    }
    failures +=
        mylite_test_expect_int(stored_value, reopen_marker_value, "legacy v1 payload value");

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_recovers_incomplete_lifecycle_files(void) {
    static const unsigned char corrupt_header_byte = 'X';

    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char header_byte = 0U;
    unsigned char state = MYLITE_FILE_LIFECYCLE_INITIALIZING;
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    long invalid_size = 0;
    long stored_size = 0;
    int stored_value = 0;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "empty_existing") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += write_file_bytes(path, "", 0U);
    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_ERROR,
        "reject empty existing file"
    );
    failures += mylite_test_expect_true(database == NULL, "empty existing file leaves output null");
    remove_related_files(path);

    if (mylite_test_make_path(path, sizeof(path), "preamble_only") != 0) {
        return failures + 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(preamble);
    failures += write_file_bytes(path, preamble, sizeof(preamble));
    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_ERROR,
        "reject preamble-only file"
    );
    failures += mylite_test_expect_true(database == NULL, "preamble-only file leaves output null");
    failures += file_size(path, &stored_size);
    failures +=
        expect_long(stored_size, MYLITE_FILE_PREAMBLE_SIZE, "preamble-only file remains unchanged");
    remove_related_files(path);

    if (mylite_test_make_path(path, sizeof(path), "initializing_preamble_only") != 0) {
        return failures + 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init_with_state(preamble, MYLITE_FILE_LIFECYCLE_INITIALIZING);
    failures += write_file_bytes(path, preamble, sizeof(preamble));
    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "recover initializing preamble-only file"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += query_single_text_equals(sqlite, "PRAGMA integrity_check", "ok");
    }
    mylite_close(database);
    database = NULL;
    failures += read_file_at(path, 0L, preamble, sizeof(preamble));
    failures += mylite_test_expect_int(
        (int)mylite_file_preamble_lifecycle_state(preamble),
        MYLITE_FILE_LIFECYCLE_COMMITTED,
        "preamble-only recovery publishes committed state"
    );
    remove_related_files(path);

    if (mylite_test_make_path(path, sizeof(path), "lifecycle") != 0) {
        return failures + 1;
    }
    remove_related_files(path);
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "create lifecycle file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += execute_sql(
            sqlite,
            "CREATE TABLE lifecycle_marker(value INTEGER);"
            "INSERT INTO lifecycle_marker(value) VALUES (73)"
        );
    }
    mylite_close(database);
    database = NULL;

    failures += write_file_at(path, MYLITE_FILE_LIFECYCLE_STATE_OFFSET, &state, sizeof(state));
    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "recover initialized payload from initializing state"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += query_single_int(sqlite, "SELECT value FROM lifecycle_marker", &stored_value);
    }
    failures += mylite_test_expect_int(
        stored_value,
        reopen_marker_value,
        "initializing-state recovery preserves payload"
    );
    mylite_close(database);
    database = NULL;
    failures += read_file_at(path, 0L, preamble, sizeof(preamble));
    failures += mylite_test_expect_int(
        (int)mylite_file_preamble_lifecycle_state(preamble),
        MYLITE_FILE_LIFECYCLE_COMMITTED,
        "initializing-state recovery publishes committed state"
    );

    state = MYLITE_FILE_LIFECYCLE_RECOVERY_REQUIRED;
    failures += write_file_at(path, MYLITE_FILE_LIFECYCLE_STATE_OFFSET, &state, sizeof(state));
    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "recover initialized payload from recovery-required state"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        stored_value = 0;
        failures += query_single_int(sqlite, "SELECT value FROM lifecycle_marker", &stored_value);
    }
    failures += mylite_test_expect_int(
        stored_value,
        reopen_marker_value,
        "recovery-required open preserves payload"
    );
    mylite_close(database);
    database = NULL;
    failures += read_file_at(path, 0L, preamble, sizeof(preamble));
    failures += mylite_test_expect_int(
        (int)mylite_file_preamble_lifecycle_state(preamble),
        MYLITE_FILE_LIFECYCLE_COMMITTED,
        "recovery-required open publishes committed state"
    );

    failures += file_size(path, &invalid_size);
    state = MYLITE_FILE_LIFECYCLE_RECOVERY_REQUIRED;
    failures += write_file_at(path, MYLITE_FILE_LIFECYCLE_STATE_OFFSET, &state, sizeof(state));
    failures += write_file_at(
        path,
        MYLITE_FILE_SQLITE_PAYLOAD_OFFSET,
        &corrupt_header_byte,
        sizeof(corrupt_header_byte)
    );
    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_ERROR,
        "reject invalid unpublished payload"
    );
    failures +=
        mylite_test_expect_true(database == NULL, "invalid unpublished payload output stays null");
    failures += file_size(path, &stored_size);
    failures +=
        expect_long(stored_size, invalid_size, "invalid unpublished payload is not truncated");
    failures +=
        read_file_at(path, MYLITE_FILE_SQLITE_PAYLOAD_OFFSET, &header_byte, sizeof(header_byte));
    failures += mylite_test_expect_int(
        (int)header_byte,
        (int)corrupt_header_byte,
        "invalid unpublished payload is not rewritten"
    );
    failures += read_file_at(path, 0L, preamble, sizeof(preamble));
    failures += mylite_test_expect_int(
        (int)mylite_file_preamble_lifecycle_state(preamble),
        MYLITE_FILE_LIFECYCLE_RECOVERY_REQUIRED,
        "invalid unpublished payload remains recovery required"
    );

    remove_related_files(path);
    return failures;
}

static int test_rejects_second_opener_during_initialization(void) {
    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE];
    char path[test_path_capacity];
    mylite_db *second = NULL;
    sqlite3 *initializing = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "concurrent_initialization") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(
        mylite_storage_open_sqlite_payload(path, &initializing),
        MYLITE_OK,
        "open initializing payload owner"
    );
    failures += mylite_test_expect_int(
        mylite_open(path, &second),
        MYLITE_ERROR,
        "reject second initialization opener"
    );
    failures += mylite_test_expect_true(second == NULL, "second initialization opener stays null");
    mylite_storage_abort_sqlite_initialization(initializing);
    failures +=
        mylite_test_expect_int(sqlite3_close(initializing), SQLITE_OK, "close initializing owner");
    failures += read_file_at(path, 0L, preamble, sizeof(preamble));
    failures += mylite_test_expect_int(
        (int)mylite_file_preamble_lifecycle_state(preamble),
        MYLITE_FILE_LIFECYCLE_RECOVERY_REQUIRED,
        "aborted owner marks recovery required"
    );
    failures += mylite_test_expect_int(
        mylite_open(path, &second),
        MYLITE_OK,
        "recover after initialization owner aborts"
    );
    mylite_close(second);
    second = NULL;
    failures += read_file_at(path, 0L, preamble, sizeof(preamble));
    failures += mylite_test_expect_int(
        (int)mylite_file_preamble_lifecycle_state(preamble),
        MYLITE_FILE_LIFECYCLE_COMMITTED,
        "post-abort recovery publishes committed state"
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

    if (mylite_test_make_path(path, sizeof(path), "concurrent_process_initialization") != 0) {
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
        _exit(initialization_child_main(&(const struct initialization_child_paths){
            .database = path,
            .ready = ready_path,
            .release = release_path,
        }));
    }
#endif
    if (child == -1) {
        fprintf(stderr, "spawn concurrent initialization owner failed\n");
        failures += 1;
    } else {
        failures += wait_for_path(ready_path);
        if (failures == 0) {
            failures += mylite_test_expect_int(
                mylite_open(path, &second),
                MYLITE_ERROR,
                "reject concurrent process initialization opener"
            );
            failures += mylite_test_expect_true(
                second == NULL,
                "concurrent process initialization opener stays null"
            );
        }
        failures += write_file_bytes(release_path, "", 0U);
#ifdef _WIN32
        failures += mylite_test_expect_true(
            _cwait(&child_status, child, 0) != -1,
            "wait for concurrent initialization owner"
        );
        failures +=
            mylite_test_expect_int(child_status, 0, "concurrent initialization owner status");
#else
        failures += mylite_test_expect_true(
            waitpid(child, &child_status, 0) == child,
            "wait for concurrent initialization owner"
        );
        if (WIFEXITED(child_status)) {
            failures += mylite_test_expect_int(
                WEXITSTATUS(child_status),
                0,
                "concurrent initialization owner status"
            );
        } else {
            failures += 1;
        }
#endif
    }

    failures += mylite_test_expect_int(
        mylite_open(path, &second),
        MYLITE_OK,
        "recover after concurrent initialization owner exits"
    );
    mylite_close(second);
    second = NULL;
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

    if (mylite_test_make_path(path, sizeof(path), "vfs_faults") != 0) {
        return 1;
    }
    remove_related_files(path);

    mylite_storage_vfs_test_set_fault(MYLITE_STORAGE_VFS_FAULT_CREATE, 1U);
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_ERROR, "inject create failure");
    failures += mylite_test_expect_true(database == NULL, "create failure leaves output null");
    failures += mylite_test_expect_true(
        mylite_storage_vfs_test_fault_was_triggered(),
        "create failpoint triggered"
    );
    failures += mylite_test_expect_int(path_exists(path), 0, "create failure leaves no file");
    mylite_storage_vfs_test_clear_fault();

    mylite_storage_vfs_test_set_fault(MYLITE_STORAGE_VFS_FAULT_WRITE, 1U);
    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "recover from one-shot preamble write failure"
    );
    failures += mylite_test_expect_true(
        mylite_storage_vfs_test_fault_was_triggered(),
        "write failpoint triggered"
    );
    mylite_close(database);
    database = NULL;
    mylite_storage_vfs_test_clear_fault();
    failures += read_file_at(path, 0L, preamble, sizeof(preamble));
    failures += mylite_test_expect_int(
        (int)mylite_file_preamble_lifecycle_state(preamble),
        MYLITE_FILE_LIFECYCLE_COMMITTED,
        "write failure recovery publishes committed state"
    );
    remove_related_files(path);

    mylite_storage_vfs_test_set_fault(MYLITE_STORAGE_VFS_FAULT_SYNC, 1U);
    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "recover from one-shot preamble sync failure"
    );
    failures += mylite_test_expect_true(
        mylite_storage_vfs_test_fault_was_triggered(),
        "sync failpoint triggered"
    );
    mylite_close(database);
    database = NULL;
    mylite_storage_vfs_test_clear_fault();
    failures += read_file_at(path, 0L, preamble, sizeof(preamble));
    failures += mylite_test_expect_int(
        (int)mylite_file_preamble_lifecycle_state(preamble),
        MYLITE_FILE_LIFECYCLE_COMMITTED,
        "sync failure recovery publishes committed state"
    );
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "create VFS fault file");
    mylite_close(database);
    database = NULL;

    mylite_storage_vfs_test_set_fault(MYLITE_STORAGE_VFS_FAULT_OPEN, 1U);
    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_ERROR,
        "inject existing open failure"
    );
    failures += mylite_test_expect_true(database == NULL, "open failure leaves output null");
    failures += mylite_test_expect_true(
        mylite_storage_vfs_test_fault_was_triggered(),
        "open failpoint triggered"
    );
    mylite_storage_vfs_test_clear_fault();

    failures += mylite_test_expect_int(
        mylite_storage_open_sqlite_payload(path, &sqlite),
        MYLITE_OK,
        "open payload for direct VFS faults"
    );
    if (sqlite != NULL) {
        failures += mylite_test_expect_int(
            sqlite3_file_control(sqlite, "main", SQLITE_FCNTL_FILE_POINTER, (void *)&file),
            SQLITE_OK,
            "load direct VFS file"
        );
    }
    if (file != NULL) {
        failures += mylite_test_expect_int(
            file->pMethods->xFileSize(file, &logical_size),
            SQLITE_OK,
            "VFS size"
        );

        mylite_storage_vfs_test_set_fault(MYLITE_STORAGE_VFS_FAULT_WRITE, 1U);
        failures += mylite_test_expect_int(
            file->pMethods->xWrite(file, preamble, 1, 0),
            SQLITE_IOERR_WRITE,
            "inject VFS write failure"
        );
        failures += mylite_test_expect_true(
            mylite_storage_vfs_test_fault_was_triggered(),
            "direct write failpoint triggered"
        );

        mylite_storage_vfs_test_set_fault(MYLITE_STORAGE_VFS_FAULT_SYNC, 1U);
        failures += mylite_test_expect_int(
            file->pMethods->xSync(file, SQLITE_SYNC_FULL),
            SQLITE_IOERR_FSYNC,
            "inject VFS sync failure"
        );
        failures += mylite_test_expect_true(
            mylite_storage_vfs_test_fault_was_triggered(),
            "direct sync failpoint triggered"
        );

        mylite_storage_vfs_test_set_fault(MYLITE_STORAGE_VFS_FAULT_TRUNCATE, 1U);
        failures += mylite_test_expect_int(
            file->pMethods->xTruncate(file, logical_size),
            SQLITE_IOERR_TRUNCATE,
            "inject VFS truncate failure"
        );
        failures += mylite_test_expect_true(
            mylite_storage_vfs_test_fault_was_triggered(),
            "truncate failpoint triggered"
        );
    }

    mylite_storage_vfs_test_set_fault(MYLITE_STORAGE_VFS_FAULT_CLOSE, 1U);
    close_rc = sqlite3_close(sqlite);
    sqlite = NULL;
    failures += mylite_test_expect_true(
        mylite_storage_vfs_test_fault_was_triggered(),
        "close failpoint triggered"
    );
    failures += mylite_test_expect_true(
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
            failures += mylite_test_expect_int(
                vfs->xDelete(vfs, delete_path, 0),
                SQLITE_IOERR_DELETE,
                "inject VFS delete failure"
            );
            failures += mylite_test_expect_true(
                mylite_storage_vfs_test_fault_was_triggered(),
                "delete failpoint triggered"
            );
            failures +=
                mylite_test_expect_int(path_exists(delete_path), 1, "failed delete preserves file");
            mylite_storage_vfs_test_clear_fault();
            (void)remove(delete_path);
        }
    }

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen after VFS faults");
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
    sqlite3_int64 mmap_size = lock_gap_control_size;
    long physical_size = 0;
    int chunk_size = lock_gap_control_size;
    int characteristics = 0;
    int failures = 0;
    size_t page_index = 0U;

    if (mylite_test_make_path(path, sizeof(path), "lock_gap") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "create lock-gap file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += mylite_test_expect_int(
            sqlite3_file_control(sqlite, "main", SQLITE_FCNTL_FILE_POINTER, (void *)&main_file),
            SQLITE_OK,
            "get lock-gap main file"
        );
    }
    if (main_file != NULL) {
        failures += mylite_test_expect_int(
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
            failures += mylite_test_expect_int(
                main_file->pMethods->xWrite(
                    main_file,
                    page,
                    page_size,
                    MYLITE_FILE_LOCK_GAP_LOGICAL_OFFSET - (page_size / 2)
                ),
                SQLITE_OK,
                "write supported page size across logical lock split"
            );
            failures += mylite_test_expect_int(
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
        failures += mylite_test_expect_int(
            main_file->pMethods->xWrite(
                main_file,
                crossing_bytes,
                crossing_size,
                MYLITE_FILE_LOCK_GAP_LOGICAL_OFFSET - 2
            ),
            SQLITE_OK,
            "write across logical lock split"
        );
        failures += mylite_test_expect_int(
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
        failures += mylite_test_expect_int(
            main_file->pMethods->xFileSize(main_file, &logical_size),
            SQLITE_OK,
            "read sparse logical size"
        );
        failures += expect_int64(
            logical_size,
            MYLITE_FILE_LOCK_GAP_LOGICAL_OFFSET + (maximum_page_size / 2),
            "sparse logical size"
        );
        failures += mylite_test_expect_int(
            main_file->pMethods->xFileControl(main_file, SQLITE_FCNTL_MMAP_SIZE, &mmap_size),
            SQLITE_OK,
            "disable mapped I/O"
        );
        failures += expect_int64(mmap_size, 0, "mapped I/O size remains zero");
        characteristics = main_file->pMethods->xDeviceCharacteristics(main_file);
        failures += mylite_test_expect_int(
            characteristics & atomic_capabilities,
            0,
            "shifted VFS clears atomic-write capabilities"
        );
        failures += mylite_test_expect_true(
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

    if (mylite_test_make_path(path, sizeof(path), "legacy_lock_limit") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "create legacy-limit file");
    mylite_close(database);
    database = NULL;
    failures +=
        write_file_at(path, MYLITE_FILE_FORMAT_VERSION_OFFSET, version_two, sizeof(version_two));

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open version-two file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += mylite_test_expect_int(
            sqlite3_file_control(sqlite, "main", SQLITE_FCNTL_FILE_POINTER, (void *)&main_file),
            SQLITE_OK,
            "get legacy main file"
        );
    }
    if (main_file != NULL) {
        failures += mylite_test_expect_int(
            main_file->pMethods->xWrite(main_file, &marker, 1, MYLITE_FILE_LEGACY_MAX_LOGICAL_SIZE),
            SQLITE_FULL,
            "reject legacy write into physical lock range"
        );
        failures += mylite_test_expect_int(
            main_file->pMethods->xTruncate(main_file, MYLITE_FILE_LEGACY_MAX_LOGICAL_SIZE + 1),
            SQLITE_FULL,
            "reject legacy truncate into physical lock range"
        );
    }
    mylite_close(database);
    database = NULL;

    failures += write_file_at(path, MYLITE_FILE_PHYSICAL_LOCK_BYTE, &marker, sizeof(marker));
    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_ERROR,
        "reject oversized version-two file"
    );
    failures +=
        mylite_test_expect_true(database == NULL, "oversized version-two output remains null");

    remove_related_files(path);
    return failures;
}

static int test_journal_mode_policy(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int synchronous = 0;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "journal_policy") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open journal-policy file");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += query_single_text_equals(sqlite, "PRAGMA journal_mode", "delete");
        failures += query_single_int(sqlite, "PRAGMA synchronous", &synchronous);
        failures += mylite_test_expect_int(
            synchronous,
            sqlite_synchronous_extra,
            "new file uses EXTRA synchronization"
        );
        failures += query_single_text_equals(sqlite, "PRAGMA journal_mode=WAL", "delete");
        failures += query_single_text_equals(sqlite, "PRAGMA journal_mode", "delete");
    }

    mylite_close(database);
    database = NULL;
    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "reopen journal-policy file"
    );
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        synchronous = 0;
        failures += query_single_int(sqlite, "PRAGMA synchronous", &synchronous);
        failures += mylite_test_expect_int(
            synchronous,
            sqlite_synchronous_extra,
            "reopened file uses EXTRA synchronization"
        );
    }
    mylite_close(database);
    failures +=
        mylite_test_expect_int(file_exists_with_suffix(path, "-wal"), 0, "WAL file is not created");
    failures += mylite_test_expect_int(
        file_exists_with_suffix(path, "-shm"),
        0,
        "shared-memory file is not created"
    );

    remove_related_files(path);
    return failures;
}

static int test_recovers_after_initialization_process_death(const char *executable_path) {
    static const char create_database_sql[] = "CREATE DATABASE recovered";
    static const char use_database_sql[] = "USE recovered";
    static const char create_table_sql[] = "CREATE TABLE viability (id INT)";

    static const enum mylite_file_initialization_test_event events[] = {
        MYLITE_FILE_INITIALIZATION_PAYLOAD_OPENED,
        MYLITE_FILE_INITIALIZATION_CATALOG_TRANSACTION_ACTIVE,
        MYLITE_FILE_INITIALIZATION_CATALOG_TRANSACTION_COMMITTED,
        MYLITE_FILE_INITIALIZATION_BEFORE_LIFECYCLE_PUBLICATION,
        MYLITE_FILE_INITIALIZATION_AFTER_LIFECYCLE_PUBLICATION,
    };

    unsigned char preamble[MYLITE_FILE_PREAMBLE_SIZE];
    char base_path[test_path_capacity];
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    sqlite3 *sqlite = NULL;
    int catalog_state_count = 0;
    int recovered_table_count = 0;
    int failures = 0;
    int written = 0;

    if (mylite_test_make_path(base_path, sizeof(base_path), "initialization_process_death") != 0) {
        return 1;
    }

    for (size_t index = 0U; index < sizeof(events) / sizeof(events[0U]); ++index) {
        enum mylite_file_initialization_test_event event = events[index];
        enum mylite_file_lifecycle_state expected_interrupted_state =
            event == MYLITE_FILE_INITIALIZATION_AFTER_LIFECYCLE_PUBLICATION
                ? MYLITE_FILE_LIFECYCLE_COMMITTED
                : MYLITE_FILE_LIFECYCLE_INITIALIZING;

        written = snprintf(path, sizeof(path), "%s-%d", base_path, (int)event);
        if (written < 0 || (size_t)written >= sizeof(path)) {
            failures += 1;
            continue;
        }
        remove_related_files(path);

        failures += run_initialization_death_child(
            &(const struct child_process_paths){
                .executable = executable_path,
                .database = path,
            },
            event
        );
        failures += read_file_at(path, 0L, preamble, sizeof(preamble));
        failures += mylite_test_expect_int(
            (int)mylite_file_preamble_lifecycle_state(preamble),
            (int)expected_interrupted_state,
            "process death preserves expected lifecycle state"
        );
        if (event == MYLITE_FILE_INITIALIZATION_CATALOG_TRANSACTION_ACTIVE) {
            failures += mylite_test_expect_int(
                file_exists_with_suffix(path, "-journal"),
                1,
                "catalog transaction death leaves rollback journal"
            );
        }

        failures += mylite_test_expect_int(
            mylite_open(path, &database),
            MYLITE_OK,
            "recover after initialization process death"
        );
        sqlite = mylite_connection_sqlite_for_test(database);
        if (sqlite != NULL) {
            failures += query_single_text_equals(sqlite, "PRAGMA integrity_check", "ok");
            failures += query_single_int(
                sqlite,
                "SELECT count(*) FROM _mylite_catalog_state",
                &catalog_state_count
            );
        }
        failures +=
            mylite_test_expect_int(catalog_state_count, 1, "recovered catalog state is complete");
        if (database != NULL) {
            failures += mylite_test_expect_int(
                mylite_execute(
                    database,
                    create_database_sql,
                    sizeof(create_database_sql) - 1U,
                    &result
                ),
                MYLITE_OK,
                "create database after initialization recovery"
            );
            mylite_result_free(result);
            result = NULL;
            failures += mylite_test_expect_int(
                mylite_execute(database, use_database_sql, sizeof(use_database_sql) - 1U, &result),
                MYLITE_OK,
                "select database after initialization recovery"
            );
            mylite_result_free(result);
            result = NULL;
            failures += mylite_test_expect_int(
                mylite_execute(database, create_table_sql, sizeof(create_table_sql) - 1U, &result),
                MYLITE_OK,
                "create table after initialization recovery"
            );
            mylite_result_free(result);
            result = NULL;
        }
        mylite_close(database);
        database = NULL;
        catalog_state_count = 0;

        failures += read_file_at(path, 0L, preamble, sizeof(preamble));
        failures += mylite_test_expect_int(
            (int)mylite_file_preamble_lifecycle_state(preamble),
            MYLITE_FILE_LIFECYCLE_COMMITTED,
            "process-death recovery publishes committed state"
        );
        failures += mylite_test_expect_int(
            file_exists_with_suffix(path, "-journal"),
            0,
            "process-death recovery clears rollback journal"
        );
        failures += mylite_test_expect_int(
            mylite_open(path, &database),
            MYLITE_OK,
            "reopen database written after initialization recovery"
        );
        sqlite = mylite_connection_sqlite_for_test(database);
        if (sqlite != NULL) {
            failures += query_single_text_equals(sqlite, "PRAGMA integrity_check", "ok");
            failures += query_single_int(
                sqlite,
                "SELECT count(*) FROM _mylite_catalog_tables WHERE name = 'viability'",
                &recovered_table_count
            );
        }
        failures += mylite_test_expect_int(
            recovered_table_count,
            1,
            "post-recovery table remains cataloged after reopen"
        );
        mylite_close(database);
        database = NULL;
        recovered_table_count = 0;

        remove_related_files(path);
    }
    return failures;
}

#ifndef _WIN32
static int test_abort_marks_opened_identity_recovery_required(void) {
    static const unsigned char replacement[] = "replacement database path";

    unsigned char owned_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char replacement_readback[sizeof(replacement)];
    char owned_path[test_path_capacity];
    char path[test_path_capacity];
    sqlite3 *initializing = NULL;
    int failures = 0;
    int written = 0;

    if (mylite_test_make_path(path, sizeof(path), "identity_replacement") != 0) {
        return 1;
    }
    written = snprintf(owned_path, sizeof(owned_path), "%s-owned", path);
    if (written < 0 || (size_t)written >= sizeof(owned_path)) {
        return 1;
    }
    remove_related_files(path);
    remove_related_files(owned_path);

    failures += mylite_test_expect_int(
        mylite_storage_open_sqlite_payload(path, &initializing),
        MYLITE_OK,
        "open identity owner"
    );
    failures += mylite_test_expect_int(rename(path, owned_path), 0, "rename opened identity");
    failures += write_file_bytes(path, replacement, sizeof(replacement));
    mylite_storage_abort_sqlite_initialization(initializing);
    failures += mylite_test_expect_int(
        sqlite3_close(initializing),
        SQLITE_OK,
        "close renamed identity owner"
    );

    failures += read_file_at(path, 0L, replacement_readback, sizeof(replacement_readback));
    failures += expect_bytes(
        replacement_readback,
        replacement,
        sizeof(replacement),
        "replacement path remains unchanged"
    );
    failures += read_file_at(owned_path, 0L, owned_preamble, sizeof(owned_preamble));
    failures += mylite_test_expect_int(
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

    if (mylite_test_make_path(link_path, sizeof(link_path), "symlink_identity") != 0) {
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
    failures += mylite_test_expect_int(symlink_rc, 0, "create database symlink");
    if (symlink_rc != 0) {
        remove_related_files(link_path);
        remove_related_files(target_path);
        return failures;
    }

    failures += mylite_test_expect_int(
        mylite_open(link_path, &database),
        MYLITE_ERROR,
        "reject symlink target"
    );
    failures += mylite_test_expect_true(database == NULL, "symlink failure leaves output null");
    failures +=
        mylite_test_expect_int(path_is_symlink(link_path), 1, "failed open preserves symlink");
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

static int test_hot_journal_recovery_after_process_death(const char *executable_path) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int changed_rows = -1;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "hot_journal") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "create recovery file");
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

    failures += run_hot_journal_child(&(const struct child_process_paths){
        .executable = executable_path,
        .database = path,
    });
    failures += mylite_test_expect_int(
        file_exists_with_suffix(path, "-journal"),
        1,
        "interrupted transaction leaves rollback journal"
    );

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "recover hot journal");
    sqlite = mylite_connection_sqlite_for_test(database);
    if (sqlite != NULL) {
        failures += query_single_int(
            sqlite,
            "SELECT count(*) FROM recovery_rows WHERE value != zeroblob(4096)",
            &changed_rows
        );
    }
    failures +=
        mylite_test_expect_int(changed_rows, 0, "hot-journal rollback restores committed rows");
    mylite_close(database);
    failures += mylite_test_expect_int(
        file_exists_with_suffix(path, "-journal"),
        0,
        "hot rollback journal removed after recovery"
    );

    remove_related_files(path);
    return failures;
}

static int initialization_child_main(const struct initialization_child_paths *paths) {
    sqlite3 *initializing = NULL;
    int rc = mylite_storage_open_sqlite_payload(paths->database, &initializing);

    if (rc != MYLITE_OK || initializing == NULL) {
        return 1;
    }
    if (write_file_bytes(paths->ready, "", 0U) != 0) {
        mylite_storage_abort_sqlite_initialization(initializing);
        (void)sqlite3_close(initializing);
        return 1;
    }
    if (wait_for_path(paths->release) != 0) {
        mylite_storage_abort_sqlite_initialization(initializing);
        (void)sqlite3_close(initializing);
        return 1;
    }

    mylite_storage_abort_sqlite_initialization(initializing);
    return sqlite3_close(initializing) == SQLITE_OK ? 0 : 1;
}

static int initialization_death_child_main(
    const char *path,
    enum mylite_file_initialization_test_event target_event
) {
    mylite_db *database = NULL;
    int rc = MYLITE_OK;

    mylite_connection_set_file_initialization_test_hook(
        initialization_death_test_hook,
        &target_event
    );
    rc = mylite_open(path, &database);
    mylite_connection_set_file_initialization_test_hook(NULL, NULL);
    mylite_close(database);

    return rc == MYLITE_OK ? 1 : 2;
}

static void initialization_death_test_hook(
    enum mylite_file_initialization_test_event event,
    void *context
) {
    const enum mylite_file_initialization_test_event *target_event = context;

    if (target_event != NULL && event == *target_event) {
        _Exit(0);
    }
}

static int run_initialization_death_child(
    const struct child_process_paths *paths,
    enum mylite_file_initialization_test_event event
) {
#ifdef _WIN32
    char event_text[16];
    intptr_t child_status = -1;
    int written = snprintf(event_text, sizeof(event_text), "%d", (int)event);

    if (written < 0 || (size_t)written >= sizeof(event_text)) {
        return 1;
    }
    child_status = _spawnl(
        _P_WAIT,
        paths->executable,
        paths->executable,
        "--initialization-death-child",
        paths->database,
        event_text,
        NULL
    );
    if (child_status == -1) {
        fprintf(stderr, "spawn initialization death child failed\n");
        return 1;
    }
    return mylite_test_expect_int(
        (int)child_status,
        0,
        "initialization death child reached target boundary"
    );
#else
    pid_t child = fork();
    int child_status = 0;

    (void)paths->executable;
    if (child == 0) {
        _exit(initialization_death_child_main(paths->database, event));
    }
    if (child < 0) {
        fprintf(stderr, "fork initialization death child failed\n");
        return 1;
    }
    if (waitpid(child, &child_status, 0) != child) {
        fprintf(stderr, "wait for initialization death child failed\n");
        return 1;
    }
    if (!WIFEXITED(child_status)) {
        fprintf(stderr, "initialization death child did not exit normally\n");
        return 1;
    }
    return mylite_test_expect_int(
        WEXITSTATUS(child_status),
        0,
        "initialization death child reached target boundary"
    );
#endif
}

static int hot_journal_child_main(const char *path) {
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int rc = mylite_open(path, &database);

    if (rc == MYLITE_OK) {
        sqlite = mylite_connection_sqlite_for_test(database);
        rc = sqlite3_exec(
            sqlite,
            "PRAGMA cache_size=1;"
            "PRAGMA cache_spill=ON;"
            "BEGIN IMMEDIATE;"
            "UPDATE recovery_rows SET value = randomblob(4096);",
            NULL,
            NULL,
            NULL
        );
    }

    return rc == SQLITE_OK ? 0 : 1;
}

static int run_hot_journal_child(const struct child_process_paths *paths) {
#ifdef _WIN32
    intptr_t child_status = _spawnl(
        _P_WAIT,
        paths->executable,
        paths->executable,
        "--hot-journal-child",
        paths->database,
        NULL
    );

    if (child_status == -1) {
        fprintf(stderr, "spawn hot-journal child failed\n");
        return 1;
    }
    return mylite_test_expect_int((int)child_status, 0, "hot-journal child updated rows");
#else
    pid_t child = fork();
    int child_status = 0;

    (void)paths->executable;
    if (child == 0) {
        _exit(hot_journal_child_main(paths->database));
    }
    if (child < 0) {
        fprintf(stderr, "fork hot-journal child failed\n");
        return 1;
    }
    if (waitpid(child, &child_status, 0) != child) {
        fprintf(stderr, "wait for hot-journal child failed\n");
        return 1;
    }
    if (!WIFEXITED(child_status)) {
        fprintf(stderr, "hot-journal child did not exit normally\n");
        return 1;
    }
    return mylite_test_expect_int(WEXITSTATUS(child_status), 0, "hot-journal child updated rows");
#endif
}

static int wait_for_path(const char *path) {
    for (int attempt = 0; attempt < path_wait_attempt_count; ++attempt) {
        if (path_exists(path)) {
            return 0;
        }
        (void)sqlite3_sleep(path_wait_sleep_ms);
    }

    fprintf(stderr, "timed out waiting for path: %s\n", path);
    return 1;
}

static int path_exists(const char *path) {
#ifdef _WIN32
    wchar_t wide_path[test_path_capacity];

    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            path,
            -1,
            wide_path,
            test_path_capacity
        ) == 0) {
        return 0;
    }
    return GetFileAttributesW(wide_path) != INVALID_FILE_ATTRIBUTES;
#else
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        return 0;
    }
    (void)fclose(file);
    return 1;
#endif
}

static void remove_related_files(const char *path) {
    remove_path(path);
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

    remove_path(related_path);
}

static void remove_path(const char *path) {
#ifdef _WIN32
    wchar_t wide_path[test_path_capacity];

    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            path,
            -1,
            wide_path,
            test_path_capacity
        ) != 0) {
        (void)DeleteFileW(wide_path);
    }
#else
    (void)remove(path);
#endif
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
