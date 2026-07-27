#include <mylite/mylite.h>

#include "mylite_test_support.h"

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_catalog_internal.h"
#include "runtime/mylite_connection.h"
#include "sqlite3.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#  include <windows.h>
#else
#  include <pthread.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    child_argument_count = 5,
    path_wait_attempt_count = 5000,
    path_wait_sleep_ms = 1,
    concurrent_opener_count = 2,
};

struct thread_barrier {
    atomic_int arrivals;
    atomic_bool released;
};

struct thread_open_context {
    const char *path;
    mylite_db *database;
    int rc;
};

struct process_hook_context {
    const char *ready_path;
    const char *release_path;
};

static int test_two_handle_migration_converges(void);
static int test_two_process_migration_converges(const char *executable_path);
static int migration_child_main(
    const char *database_path,
    const char *ready_path,
    const char *release_path
);
static void await_thread_barrier(void *context);
static void await_process_release(void *context);
#ifdef _WIN32
static unsigned __stdcall open_database_thread(void *argument);
#else
static void *open_database_thread(void *argument);
#endif
static int prepare_v37_database(const char *path);
static int validate_current_database(mylite_db *database, const char *context);
static int query_single_int(sqlite3 *sqlite, const char *sql, int *out_value);
static int execute_sql(sqlite3 *sqlite, const char *sql);
static int write_empty_file(const char *path);
static int wait_for_path(const char *path);
static int path_exists(const char *path);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);

int main(int argc, char **argv) {
    int failures = 0;

    if (argc == child_argument_count && strcmp(argv[1], "--migration-child") == 0) {
        return migration_child_main(argv[2], argv[3], argv[4]);
    }

    failures += test_two_handle_migration_converges();
    failures += test_two_process_migration_converges(argv[0]);
    return failures == 0 ? 0 : 1;
}

static int test_two_handle_migration_converges(void) {
    char path[test_path_capacity];
    struct thread_barrier barrier = {0};
    struct thread_open_context openers[concurrent_opener_count] = {0};
    int failures = 0;
#ifdef _WIN32
    uintptr_t threads[concurrent_opener_count] = {0};
#else
    pthread_t threads[concurrent_opener_count];
    bool threads_created[concurrent_opener_count] = {false};
#endif

    if (mylite_test_make_path(path, sizeof(path), "catalog_migration_threads") != 0) {
        return 1;
    }
    atomic_init(&barrier.arrivals, 0);
    atomic_init(&barrier.released, false);
    remove_related_files(path);
    failures += prepare_v37_database(path);
    mylite_catalog_set_migration_prelock_test_hook(await_thread_barrier, &barrier);

    for (int index = 0; index < concurrent_opener_count && failures == 0; ++index) {
        openers[index].path = path;
#ifdef _WIN32
        threads[index] = _beginthreadex(NULL, 0U, open_database_thread, &openers[index], 0U, NULL);
        if (threads[index] == 0U) {
            fprintf(stderr, "create catalog migration thread %d failed\n", index);
            failures++;
        }
#else
        if (pthread_create(&threads[index], NULL, open_database_thread, &openers[index]) != 0) {
            fprintf(stderr, "create catalog migration thread %d failed\n", index);
            failures++;
        } else {
            threads_created[index] = true;
        }
#endif
    }
    int wait_attempt = 0;

    while (failures == 0 && wait_attempt < path_wait_attempt_count &&
           atomic_load_explicit(&barrier.arrivals, memory_order_acquire) < concurrent_opener_count
    ) {
        (void)sqlite3_sleep(path_wait_sleep_ms);
        ++wait_attempt;
    }
    if (failures == 0 &&
        atomic_load_explicit(&barrier.arrivals, memory_order_acquire) < concurrent_opener_count) {
        fprintf(stderr, "timed out waiting for catalog migration threads\n");
        failures++;
    }
    atomic_store_explicit(&barrier.released, true, memory_order_release);

    for (int index = 0; index < concurrent_opener_count; ++index) {
#ifdef _WIN32
        if (threads[index] != 0U) {
            failures += mylite_test_expect_int(
                (int)WaitForSingleObject((HANDLE)threads[index], INFINITE),
                (int)WAIT_OBJECT_0,
                "join catalog migration thread"
            );
            (void)CloseHandle((HANDLE)threads[index]);
        }
#else
        if (threads_created[index]) {
            failures += mylite_test_expect_int(
                pthread_join(threads[index], NULL),
                0,
                "join catalog migration thread"
            );
        }
#endif
        failures += mylite_test_expect_int(
            openers[index].rc,
            MYLITE_OK,
            "concurrent catalog migration opener"
        );
        if (openers[index].database != NULL) {
            failures += validate_current_database(
                openers[index].database,
                "concurrent catalog migration handle"
            );
            mylite_close(openers[index].database);
        }
    }
    mylite_catalog_set_migration_prelock_test_hook(NULL, NULL);

    failures += mylite_test_expect_int(
        atomic_load_explicit(&barrier.arrivals, memory_order_acquire),
        concurrent_opener_count,
        "both migration handles reached pre-lock barrier"
    );
    remove_related_files(path);
    return failures;
}

static int test_two_process_migration_converges(const char *executable_path) {
    char path[test_path_capacity];
    char ready_paths[concurrent_opener_count][test_path_capacity];
    char release_path[test_path_capacity];
    int failures = 0;
#ifdef _WIN32
    intptr_t children[concurrent_opener_count] = {-1, -1};
#else
    pid_t children[concurrent_opener_count] = {-1, -1};

    (void)executable_path;
#endif

    if (mylite_test_make_path(path, sizeof(path), "catalog_migration_processes") != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += prepare_v37_database(path);
    for (int index = 0; index < concurrent_opener_count; ++index) {
        int written =
            snprintf(ready_paths[index], sizeof(ready_paths[index]), "%s-ready-%d", path, index);

        if (written < 0 || (size_t)written >= sizeof(ready_paths[index])) {
            failures++;
        }
        (void)remove(ready_paths[index]);
    }
    int written = snprintf(release_path, sizeof(release_path), "%s-release", path);

    if (written < 0 || (size_t)written >= sizeof(release_path)) {
        failures++;
    }
    (void)remove(release_path);

    for (int index = 0; index < concurrent_opener_count && failures == 0; ++index) {
#ifdef _WIN32
        children[index] = _spawnl(
            _P_NOWAIT,
            executable_path,
            executable_path,
            "--migration-child",
            path,
            ready_paths[index],
            release_path,
            NULL
        );
#else
        children[index] = fork();
        if (children[index] == 0) {
            _exit(migration_child_main(path, ready_paths[index], release_path));
        }
#endif
        if (children[index] == -1) {
            fprintf(stderr, "spawn catalog migration process %d failed\n", index);
            failures++;
        }
    }
    for (int index = 0; index < concurrent_opener_count; ++index) {
        if (children[index] != -1) {
            failures += wait_for_path(ready_paths[index]);
        }
    }
    failures += write_empty_file(release_path);

    for (int index = 0; index < concurrent_opener_count; ++index) {
        int status = -1;

        if (children[index] == -1) {
            continue;
        }
#ifdef _WIN32
        failures += mylite_test_expect_true(
            _cwait(&status, children[index], 0) != -1,
            "wait for catalog migration child"
        );
        failures += mylite_test_expect_int(status, 0, "catalog migration child status");
#else
        failures += mylite_test_expect_true(
            waitpid(children[index], &status, 0) == children[index],
            "wait for catalog migration child"
        );
        failures += mylite_test_expect_true(WIFEXITED(status), "catalog migration child exited");
        if (WIFEXITED(status)) {
            failures +=
                mylite_test_expect_int(WEXITSTATUS(status), 0, "catalog migration child status");
        }
#endif
    }
    mylite_db *database = NULL;

    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "reopen concurrently migrated catalog"
    );
    if (database != NULL) {
        failures += validate_current_database(database, "reopened concurrent migration");
        mylite_close(database);
    }

    for (int index = 0; index < concurrent_opener_count; ++index) {
        (void)remove(ready_paths[index]);
    }
    (void)remove(release_path);
    remove_related_files(path);
    return failures;
}

static int migration_child_main(
    const char *database_path,
    const char *ready_path,
    const char *release_path
) {
    struct process_hook_context hook = {
        .ready_path = ready_path,
        .release_path = release_path,
    };
    mylite_db *database = NULL;
    int failures = 0;

    mylite_catalog_set_migration_prelock_test_hook(await_process_release, &hook);
    failures += mylite_test_expect_int(
        mylite_open(database_path, &database),
        MYLITE_OK,
        "child catalog migration open"
    );
    if (database != NULL) {
        failures += validate_current_database(database, "child catalog migration");
        mylite_close(database);
    }
    mylite_catalog_set_migration_prelock_test_hook(NULL, NULL);
    return failures == 0 ? 0 : 1;
}

static void await_thread_barrier(void *context) {
    struct thread_barrier *barrier = context;

    (void)atomic_fetch_add_explicit(&barrier->arrivals, 1, memory_order_acq_rel);
    while (!atomic_load_explicit(&barrier->released, memory_order_acquire)) {
        (void)sqlite3_sleep(path_wait_sleep_ms);
    }
}

static void await_process_release(void *context) {
    struct process_hook_context *hook = context;

    if (write_empty_file(hook->ready_path) != 0) {
        return;
    }
    (void)wait_for_path(hook->release_path);
}

#ifdef _WIN32
static unsigned __stdcall open_database_thread(void *argument) {
#else
static void *open_database_thread(void *argument) {
#endif
    struct thread_open_context *opener = argument;

    opener->rc = mylite_open(opener->path, &opener->database);
#ifdef _WIN32
    return 0U;
#else
    return NULL;
#endif
}

static int prepare_v37_database(const char *path) {
    mylite_db *database = NULL;
    sqlite3 *sqlite = NULL;
    int failures =
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "create migration file");

    if (database == NULL) {
        return failures + 1;
    }
    sqlite = mylite_connection_sqlite_for_test(database);
    failures += mylite_test_remove_catalog_integrity_seal(sqlite);
    failures += execute_sql(
        sqlite,
        "UPDATE _mylite_catalog_state "
        "SET schema_version = 37, minimum_reader_schema_version = 37"
    );
    mylite_close(database);
    return failures;
}

static int validate_current_database(mylite_db *database, const char *context) {
    sqlite3 *sqlite = mylite_connection_sqlite_for_test(database);
    int schema_version = 0;
    int minimum_reader = 0;
    int integrity_generation = 0;
    int generation = 0;
    int integrity_check = 0;
    int failures = query_single_int(
        sqlite,
        "SELECT schema_version FROM _mylite_catalog_state",
        &schema_version
    );

    failures += query_single_int(
        sqlite,
        "SELECT minimum_reader_schema_version FROM _mylite_catalog_state",
        &minimum_reader
    );
    failures += query_single_int(
        sqlite,
        "SELECT catalog_generation FROM _mylite_catalog_state",
        &generation
    );
    failures += query_single_int(
        sqlite,
        "SELECT integrity_catalog_generation FROM _mylite_catalog_state",
        &integrity_generation
    );
    failures += query_single_int(
        sqlite,
        "SELECT CASE WHEN integrity_check = 'ok' THEN 1 ELSE 0 END "
        "FROM pragma_integrity_check",
        &integrity_check
    );
    failures += mylite_test_expect_int(schema_version, MYLITE_CATALOG_SCHEMA_VERSION, context);
    failures += mylite_test_expect_int(
        minimum_reader,
        MYLITE_CATALOG_MINIMUM_READER_SCHEMA_VERSION,
        context
    );
    failures += mylite_test_expect_int(integrity_generation, generation, context);
    failures += mylite_test_expect_int(integrity_check, 1, context);
    return failures;
}

static int query_single_int(sqlite3 *sqlite, const char *sql, int *out_value) {
    sqlite3_stmt *statement = NULL;
    int rc = sqlite3_prepare_v2(sqlite, sql, -1, &statement, NULL);

    if (rc == SQLITE_OK) {
        rc = sqlite3_step(statement);
    }
    if (rc == SQLITE_ROW) {
        *out_value = sqlite3_column_int(statement, 0);
        rc = sqlite3_step(statement);
    }
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "query failed: %s: %s\n", sql, sqlite3_errmsg(sqlite));
    }
    int finalize_rc = sqlite3_finalize(statement);

    return rc == SQLITE_DONE && finalize_rc == SQLITE_OK ? 0 : 1;
}

static int execute_sql(sqlite3 *sqlite, const char *sql) {
    char *error = NULL;
    int rc = sqlite3_exec(sqlite, sql, NULL, NULL, &error);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL failed: %s: %s\n", sql, error == NULL ? "" : error);
    }
    sqlite3_free(error);
    return rc == SQLITE_OK ? 0 : 1;
}

static int write_empty_file(const char *path) {
    FILE *file = fopen(path, "wb");

    if (file == NULL) {
        fprintf(stderr, "create barrier file failed: %s\n", path);
        return 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "close barrier file failed: %s\n", path);
        return 1;
    }
    return 0;
}

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
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(related_path)) {
        (void)remove(related_path);
    }
}
