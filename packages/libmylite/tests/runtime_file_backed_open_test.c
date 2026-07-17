#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "sqlite3.h"
#include "storage/mylite_file_format.h"
#include "storage/mylite_file_open.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
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
#ifndef _WIN32
static int test_process_death_leaves_initializing_file_rejected(void);
static int test_abort_marks_opened_identity_recovery_required(void);
#endif
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int write_file_bytes(const char *path, const void *bytes, size_t size);
static int write_file_at(const char *path, long offset, const void *bytes, size_t size);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int file_size(const char *path, long *out_size);
static int create_plain_sqlite_database(const char *path);
static int execute_sql(sqlite3 *connection, const char *sql);
static int query_single_int(sqlite3 *connection, const char *sql, int *out_value);
static int table_exists(sqlite3 *connection, const char *table_name, int *out_exists);
static int expect_int(int actual, int expected, const char *context);
static int expect_long(long actual, long expected, const char *context);
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

int main(void) {
    int failures = 0;

    failures += test_open_rejects_invalid_arguments();
    failures += test_create_new_file_with_preamble_and_shifted_payload();
    failures += test_reopen_existing_file_preserves_sqlite_payload();
    failures += test_rejects_invalid_truncated_and_plain_sqlite_files();
    failures += test_independent_file_backed_handles_and_bootstrap_state();
    failures += test_reopens_legacy_version_one_file();
    failures += test_rejects_incomplete_lifecycle_files();
    failures += test_rejects_second_opener_during_initialization();
#ifndef _WIN32
    failures += test_process_death_leaves_initializing_file_rejected();
    failures += test_abort_marks_opened_identity_recovery_required();
#endif

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
