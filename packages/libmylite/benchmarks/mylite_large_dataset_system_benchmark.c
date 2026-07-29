#include <mylite/mylite.h>

#include "sqlite3.h"

#include <errno.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#if defined(_WIN32)
#  include <process.h>
#  include <windows.h>
#else
#  include <pthread.h>
#  include <sched.h>
#  include <unistd.h>
#endif

enum {
    path_capacity = 1024,
    default_row_count = 100000,
    default_iterations = 100,
    parallel_reader_count = 4,
    mixed_reader_count = 4,
    maximum_worker_count = 5,
    sqlite_busy_timeout_ms = 5000,
    nanoseconds_per_second = 1000000000ULL,
    nanoseconds_per_millisecond = 1000000,
    nanoseconds_per_microsecond = 1000,
    decimal_base = 10,
    delete_divisor = 10,
    delete_sql_capacity = 128,
    copy_buffer_size = 64 * 1024,
    bits_per_byte = 8,
};

static const uint64_t fnv_offset_basis = 1469598103934665603ULL;
static const uint64_t fnv_prime = 1099511628211ULL;

enum system_engine_kind {
    system_engine_mylite,
    system_engine_sqlite,
};

struct system_options {
    const char *database_base;
    const char *output_path;
    size_t row_count;
    size_t iterations;
    bool show_help;
};

struct system_database {
    enum system_engine_kind kind;
    mylite_db *mylite;
    sqlite3 *sqlite;
};

struct system_statement {
    enum system_engine_kind kind;
    mylite_stmt *mylite;
    sqlite3_stmt *sqlite;
};

struct worker_context {
    const char *path;
    const char *sql;
    size_t iterations;
    atomic_bool *start;
    atomic_size_t *ready;
    size_t operations;
    size_t errors;
    uint64_t checksum;
    enum system_engine_kind kind;
    bool writer;
};

struct concurrency_configuration {
    const char *name;
    size_t row_count;
    size_t reader_count;
    size_t writer_count;
    size_t iterations;
    bool full_scan;
};

struct system_measurement {
    uint64_t elapsed_ns;
    uint64_t checksum;
    size_t operations;
    size_t errors;
    uint64_t bytes_before;
    uint64_t bytes_after_delete;
    uint64_t bytes_after_reclaim;
};

#if defined(_WIN32)
typedef HANDLE benchmark_thread;
#else
typedef pthread_t benchmark_thread;
#endif

static int parse_options(int argc, char **argv, struct system_options *out_options);
static void print_usage(const char *program_name, FILE *stream);
static int run_benchmark(const struct system_options *options);
static int make_database_paths(
    const struct system_options *options,
    char *mylite_path,
    size_t mylite_path_size,
    char *sqlite_path,
    size_t sqlite_path_size
);
static int run_reopen_scenario(
    FILE *output,
    enum system_engine_kind kind,
    const char *path,
    const struct system_options *options
);
static int run_concurrency_scenario(
    FILE *output,
    enum system_engine_kind kind,
    const char *path,
    const struct concurrency_configuration *configuration
);
static int start_worker(benchmark_thread *thread, struct worker_context *context);
static int join_worker(benchmark_thread thread);
#if defined(_WIN32)
static DWORD WINAPI run_worker(LPVOID argument);
#else
static void *run_worker(void *argument);
#endif
static void execute_worker(struct worker_context *context);
static int initialize_worker(
    struct worker_context *context,
    struct system_database *database,
    struct system_statement *statement
);
static void await_worker_start(const struct worker_context *context);
static int start_worker_transaction(
    struct worker_context *context,
    struct system_database *database
);
static void execute_worker_iterations(
    struct worker_context *context,
    struct system_database *database,
    struct system_statement *statement
);
static void cleanup_worker(
    struct worker_context *context,
    struct system_database *database,
    struct system_statement *statement,
    bool transaction_started
);
static const char *worker_role(const struct worker_context *context);
static const char *database_error_message(const struct system_database *database);
static int run_lifecycle_scenario(
    FILE *output,
    enum system_engine_kind kind,
    const char *source_path,
    const struct system_options *options
);
static int make_lifecycle_path(
    const char *source_path,
    enum system_engine_kind kind,
    char *out_path,
    size_t out_path_size
);
static int copy_file(const char *source, const char *destination);
static int open_database(
    enum system_engine_kind kind,
    const char *path,
    struct system_database *out_database
);
static void close_database(struct system_database *database);
static int prepare_statement(
    struct system_database *database,
    const char *sql,
    struct system_statement *out_statement
);
static int reset_statement(struct system_statement *statement);
static int step_and_hash(
    struct system_database *database,
    struct system_statement *statement,
    uint64_t *checksum
);
static int step_mylite_and_hash(
    struct system_database *database,
    struct system_statement *statement,
    uint64_t *checksum
);
static int step_sqlite_and_hash(
    struct system_database *database,
    struct system_statement *statement,
    uint64_t *checksum
);
static int finalize_statement(struct system_statement *statement);
static int execute_sql(struct system_database *database, const char *sql);
static int fetch_count(struct system_database *database, const char *sql, uint64_t *out_count);
static void print_measurement(
    FILE *output,
    const char *scenario,
    size_t row_count,
    enum system_engine_kind kind,
    size_t workers,
    size_t iterations,
    const struct system_measurement *measurement
);
static uint64_t monotonic_now_ns(void);
static uint64_t file_size(const char *path);
static uint64_t hash_bytes(uint64_t hash, const void *bytes, size_t size);
static void hash_uint64(uint64_t *hash, uint64_t value);
static const char *engine_name(enum system_engine_kind kind);
static int parse_size(const char *text, size_t *out_value);
static long process_id(void);

int main(int argc, char **argv) {
    struct system_options options = {
        .database_base = NULL,
        .output_path = NULL,
        .row_count = default_row_count,
        .iterations = default_iterations,
        .show_help = false,
    };

    if (parse_options(argc, argv, &options) != 0) {
        print_usage(argv[0], stderr);
        return 2;
    }
    if (options.show_help) {
        print_usage(argv[0], stdout);
        return 0;
    }
    return run_benchmark(&options);
}

static int parse_options(int argc, char **argv, struct system_options *out_options) {
    for (int index = 1; index < argc; ++index) {
        const char *argument = argv[index];

        if (strcmp(argument, "--help") == 0) {
            out_options->show_help = true;
            continue;
        }
        if (index + 1 >= argc) {
            return 1;
        }
        if (strcmp(argument, "--database-base") == 0) {
            out_options->database_base = argv[++index];
        } else if (strcmp(argument, "--output") == 0) {
            out_options->output_path = argv[++index];
        } else if (strcmp(argument, "--rows") == 0) {
            if (parse_size(argv[++index], &out_options->row_count) != 0) {
                return 1;
            }
        } else if (strcmp(argument, "--iterations") == 0) {
            if (parse_size(argv[++index], &out_options->iterations) != 0) {
                return 1;
            }
        } else {
            return 1;
        }
    }
    return out_options->show_help || out_options->database_base != NULL ? 0 : 1;
}

static void print_usage(const char *program_name, FILE *stream) {
    fprintf(
        stream,
        "Usage: %s --database-base PATH [--rows N] [--iterations N] [--output PATH]\n",
        program_name
    );
}

static int run_benchmark(const struct system_options *options) {
    static const enum system_engine_kind engines[] = {
        system_engine_mylite,
        system_engine_sqlite,
    };
    const struct concurrency_configuration parallel_readers = {
        .name = "parallel_readers_4",
        .row_count = options->row_count,
        .reader_count = parallel_reader_count,
        .writer_count = 0U,
        .iterations = options->iterations,
        .full_scan = false,
    };
    const struct concurrency_configuration mixed_readers_writer = {
        .name = "readers_4_writer_1",
        .row_count = options->row_count,
        .reader_count = mixed_reader_count,
        .writer_count = 1U,
        .iterations = options->iterations,
        .full_scan = false,
    };
    const struct concurrency_configuration long_reader_writer = {
        .name = "long_reader_writer",
        .row_count = options->row_count,
        .reader_count = 1U,
        .writer_count = 1U,
        .iterations = 1U,
        .full_scan = true,
    };
    char mylite_path[path_capacity];
    char sqlite_path[path_capacity];
    FILE *output = stdout;
    int result = 1;

    if (make_database_paths(
            options,
            mylite_path,
            sizeof(mylite_path),
            sqlite_path,
            sizeof(sqlite_path)
        ) != 0) {
        return 1;
    }
    if (options->output_path != NULL) {
        output = fopen(options->output_path, "wb");
        if (output == NULL) {
            fprintf(
                stderr,
                "system-benchmark: open %s failed: %s\n",
                options->output_path,
                strerror(errno)
            );
            return 1;
        }
    }
    fprintf(
        output,
        "scenario,rows,engine,workers,iterations,operations,errors,checksum,total_ms,"
        "avg_us,ops_per_sec,bytes_before,bytes_after_delete,bytes_after_reclaim\n"
    );
    for (size_t engine_index = 0U; engine_index < sizeof(engines) / sizeof(engines[0]);
         ++engine_index) {
        enum system_engine_kind kind = engines[engine_index];
        const char *path = kind == system_engine_mylite ? mylite_path : sqlite_path;

        if (run_reopen_scenario(output, kind, path, options) != 0 ||
            run_concurrency_scenario(output, kind, path, &parallel_readers) != 0 ||
            run_concurrency_scenario(output, kind, path, &mixed_readers_writer) != 0 ||
            run_concurrency_scenario(output, kind, path, &long_reader_writer) != 0 ||
            run_lifecycle_scenario(output, kind, path, options) != 0) {
            goto cleanup;
        }
    }
    result = 0;

cleanup:
    if (output != stdout && fclose(output) != 0) {
        result = 1;
    }
    return result;
}

static int make_database_paths(
    const struct system_options *options,
    char *mylite_path,
    size_t mylite_path_size,
    char *sqlite_path,
    size_t sqlite_path_size
) {
    int mylite_written =
        snprintf(mylite_path, mylite_path_size, "%s.mylite", options->database_base);
    int sqlite_written =
        snprintf(sqlite_path, sqlite_path_size, "%s.sqlite", options->database_base);

    if (mylite_written < 0 || (size_t)mylite_written >= mylite_path_size || sqlite_written < 0 ||
        (size_t)sqlite_written >= sqlite_path_size) {
        fprintf(stderr, "system-benchmark: database path is too long\n");
        return 1;
    }
    return 0;
}

static int run_reopen_scenario(
    FILE *output,
    enum system_engine_kind kind,
    const char *path,
    const struct system_options *options
) {
    struct system_measurement measurement = {.checksum = fnv_offset_basis};
    uint64_t started = monotonic_now_ns();

    for (size_t iteration = 0U; iteration < options->iterations; ++iteration) {
        struct system_database database = {0};
        uint64_t count = 0U;

        if (open_database(kind, path, &database) != 0 ||
            fetch_count(&database, "SELECT COUNT(*) FROM items", &count) != 0 ||
            count != options->row_count) {
            close_database(&database);
            return 1;
        }
        close_database(&database);
        hash_uint64(&measurement.checksum, count);
        ++measurement.operations;
    }
    measurement.elapsed_ns = monotonic_now_ns() - started;
    print_measurement(
        output,
        "warm_reopen_count",
        options->row_count,
        kind,
        1U,
        options->iterations,
        &measurement
    );
    return 0;
}

static int run_concurrency_scenario(
    FILE *output,
    enum system_engine_kind kind,
    const char *path,
    const struct concurrency_configuration *configuration
) {
    static const char bounded_read_sql[] =
        "SELECT COUNT(*), SUM(score) FROM items WHERE id BETWEEN 1 AND 1000";
    static const char full_read_sql[] = "SELECT COUNT(*), SUM(score) FROM items";
    static const char write_sql[] = "UPDATE items SET score = score + 1 WHERE id = 1";
    struct worker_context workers[maximum_worker_count] = {0};
    benchmark_thread threads[maximum_worker_count];
    struct system_measurement measurement = {.checksum = fnv_offset_basis};
    atomic_bool start = false;
    atomic_size_t ready = 0U;
    const char *read_sql = configuration->full_scan ? full_read_sql : bounded_read_sql;
    size_t worker_count = configuration->reader_count + configuration->writer_count;
    size_t started_count = 0U;
    uint64_t started = 0U;
    int result = 1;

    if (worker_count > maximum_worker_count) {
        return 1;
    }
    for (size_t index = 0U; index < worker_count; ++index) {
        bool writer = index >= configuration->reader_count;

        workers[index] = (struct worker_context){
            .kind = kind,
            .path = path,
            .sql = writer ? write_sql : read_sql,
            .iterations = configuration->iterations,
            .writer = writer,
            .start = &start,
            .ready = &ready,
            .checksum = fnv_offset_basis,
        };
        if (start_worker(&threads[index], &workers[index]) != 0) {
            atomic_store_explicit(&start, true, memory_order_release);
            goto cleanup;
        }
        ++started_count;
    }
    while (atomic_load_explicit(&ready, memory_order_acquire) < worker_count) {
#if defined(_WIN32)
        Sleep(0U);
#else
        (void)sched_yield();
#endif
    }
    started = monotonic_now_ns();
    atomic_store_explicit(&start, true, memory_order_release);
    for (size_t index = 0U; index < started_count; ++index) {
        if (join_worker(threads[index]) != 0) {
            goto cleanup_joined;
        }
    }
    started_count = 0U;
    measurement.elapsed_ns = monotonic_now_ns() - started;
    for (size_t index = 0U; index < worker_count; ++index) {
        measurement.operations += workers[index].operations;
        measurement.errors += workers[index].errors;
        hash_uint64(&measurement.checksum, workers[index].checksum);
    }
    if (measurement.errors != 0U ||
        measurement.operations != worker_count * configuration->iterations) {
        fprintf(
            stderr,
            "system-benchmark: %s %s operations=%zu errors=%zu\n",
            configuration->name,
            engine_name(kind),
            measurement.operations,
            measurement.errors
        );
        goto cleanup_joined;
    }
    print_measurement(
        output,
        configuration->name,
        configuration->row_count,
        kind,
        worker_count,
        configuration->iterations,
        &measurement
    );
    result = 0;

cleanup:
    atomic_store_explicit(&start, true, memory_order_release);
    for (size_t index = 0U; index < started_count; ++index) {
        (void)join_worker(threads[index]);
    }
cleanup_joined:
    return result;
}

static int start_worker(benchmark_thread *thread, struct worker_context *context) {
#if defined(_WIN32)
    *thread = CreateThread(NULL, 0U, run_worker, context, 0U, NULL);
    return *thread == NULL ? 1 : 0;
#else
    return pthread_create(thread, NULL, run_worker, context);
#endif
}

static int join_worker(benchmark_thread thread) {
#if defined(_WIN32)
    int result = WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0 ? 0 : 1;

    (void)CloseHandle(thread);
    return result;
#else
    return pthread_join(thread, NULL);
#endif
}

#if defined(_WIN32)
static DWORD WINAPI run_worker(LPVOID argument) {
    execute_worker(argument);
    return 0U;
}
#else
static void *run_worker(void *argument) {
    execute_worker(argument);
    return NULL;
}
#endif

static void execute_worker(struct worker_context *context) {
    struct system_database database = {0};
    struct system_statement statement = {0};
    bool transaction_started = false;

    if (initialize_worker(context, &database, &statement) == 0) {
        atomic_fetch_add_explicit(context->ready, 1U, memory_order_release);
        await_worker_start(context);
        if (start_worker_transaction(context, &database) == 0) {
            transaction_started = context->writer;
            execute_worker_iterations(context, &database, &statement);
            if (transaction_started && execute_sql(&database, "ROLLBACK") != 0) {
                ++context->errors;
            }
            transaction_started = false;
        }
    } else {
        ++context->errors;
        atomic_fetch_add_explicit(context->ready, 1U, memory_order_release);
    }
    cleanup_worker(context, &database, &statement, transaction_started);
}

static int initialize_worker(
    struct worker_context *context,
    struct system_database *database,
    struct system_statement *statement
) {
    if (open_database(context->kind, context->path, database) != 0) {
        fprintf(stderr, "system-benchmark: %s worker open failed\n", engine_name(context->kind));
        return 1;
    }
    if (prepare_statement(database, context->sql, statement) != 0) {
        fprintf(
            stderr,
            "system-benchmark: %s %s prepare failed: %s\nSQL: %s\n",
            engine_name(context->kind),
            worker_role(context),
            database_error_message(database),
            context->sql
        );
        return 1;
    }

    return 0;
}

static void await_worker_start(const struct worker_context *context) {
    while (!atomic_load_explicit(context->start, memory_order_acquire)) {
#if defined(_WIN32)
        Sleep(0U);
#else
        (void)sched_yield();
#endif
    }
}

static int start_worker_transaction(
    struct worker_context *context,
    struct system_database *database
) {
    if (!context->writer) {
        return 0;
    }
    if (execute_sql(
            database,
            context->kind == system_engine_mylite ? "START TRANSACTION" : "BEGIN"
        ) != 0) {
        ++context->errors;
        return 1;
    }

    return 0;
}

static void execute_worker_iterations(
    struct worker_context *context,
    struct system_database *database,
    struct system_statement *statement
) {
    for (size_t iteration = 0U; iteration < context->iterations; ++iteration) {
        if (reset_statement(statement) != 0) {
            fprintf(
                stderr,
                "system-benchmark: %s %s reset failed: %s\n",
                engine_name(context->kind),
                worker_role(context),
                database_error_message(database)
            );
            ++context->errors;
            break;
        }
        if (step_and_hash(database, statement, &context->checksum) != 0) {
            ++context->errors;
            break;
        }
        ++context->operations;
    }
}

static void cleanup_worker(
    struct worker_context *context,
    struct system_database *database,
    struct system_statement *statement,
    bool transaction_started
) {
    if (transaction_started) {
        (void)execute_sql(database, "ROLLBACK");
    }
    if (statement->mylite != NULL || statement->sqlite != NULL) {
        if (finalize_statement(statement) != 0) {
            fprintf(
                stderr,
                "system-benchmark: %s %s finalize failed\n",
                engine_name(context->kind),
                worker_role(context)
            );
            ++context->errors;
        }
    }
    close_database(database);
}

static const char *worker_role(const struct worker_context *context) {
    return context->writer ? "writer" : "reader";
}

static const char *database_error_message(const struct system_database *database) {
    return database->kind == system_engine_mylite ? mylite_errmsg(database->mylite)
                                                  : sqlite3_errmsg(database->sqlite);
}

static int run_lifecycle_scenario(
    FILE *output,
    enum system_engine_kind kind,
    const char *source_path,
    const struct system_options *options
) {
    char lifecycle_path[path_capacity];
    char delete_sql[delete_sql_capacity];
    struct system_database database = {0};
    struct system_measurement measurement = {.checksum = fnv_offset_basis};
    uint64_t remaining_count = 0U;
    uint64_t expected_remaining = options->row_count - (options->row_count / delete_divisor);
    int delete_written = snprintf(
        delete_sql,
        sizeof(delete_sql),
        "DELETE FROM items WHERE id <= %zu",
        options->row_count / delete_divisor
    );
    uint64_t started = 0U;
    int result = 1;

    if (delete_written < 0 || (size_t)delete_written >= sizeof(delete_sql) ||
        make_lifecycle_path(source_path, kind, lifecycle_path, sizeof(lifecycle_path)) != 0 ||
        copy_file(source_path, lifecycle_path) != 0) {
        return 1;
    }
    measurement.bytes_before = file_size(lifecycle_path);
    if (open_database(kind, lifecycle_path, &database) != 0) {
        goto cleanup;
    }
    started = monotonic_now_ns();
    if (execute_sql(&database, kind == system_engine_mylite ? "START TRANSACTION" : "BEGIN") != 0 ||
        execute_sql(&database, delete_sql) != 0 || execute_sql(&database, "COMMIT") != 0) {
        goto cleanup;
    }
    measurement.elapsed_ns = monotonic_now_ns() - started;
    measurement.operations = options->row_count / delete_divisor;
    measurement.bytes_after_delete = file_size(lifecycle_path);
    if (fetch_count(&database, "SELECT COUNT(*) FROM items", &remaining_count) != 0 ||
        remaining_count != expected_remaining) {
        goto cleanup;
    }
    started = monotonic_now_ns();
    if (kind == system_engine_mylite) {
        if (execute_sql(&database, "OPTIMIZE TABLE items") != 0) {
            goto cleanup;
        }
    } else if (execute_sql(&database, "VACUUM") != 0 ||
               execute_sql(&database, "ANALYZE items") != 0) {
        goto cleanup;
    }
    measurement.elapsed_ns += monotonic_now_ns() - started;
    measurement.bytes_after_reclaim = file_size(lifecycle_path);
    hash_uint64(&measurement.checksum, remaining_count);
    print_measurement(
        output,
        "delete_commit_reclaim",
        options->row_count,
        kind,
        1U,
        1U,
        &measurement
    );
    result = 0;

cleanup:
    close_database(&database);
    (void)remove(lifecycle_path);
    return result;
}

static int make_lifecycle_path(
    const char *source_path,
    enum system_engine_kind kind,
    char *out_path,
    size_t out_path_size
) {
    int written = snprintf(
        out_path,
        out_path_size,
        "%s.lifecycle-%s-%ld",
        source_path,
        engine_name(kind),
        process_id()
    );

    return written < 0 || (size_t)written >= out_path_size ? 1 : 0;
}

static int copy_file(const char *source, const char *destination) {
    unsigned char buffer[copy_buffer_size];
    FILE *input = fopen(source, "rb");
    FILE *output = NULL;
    int result = 1;

    if (input == NULL) {
        return 1;
    }
    output = fopen(destination, "wb");
    if (output == NULL) {
        goto cleanup;
    }
    for (;;) {
        size_t read_count = fread(buffer, 1U, sizeof(buffer), input);

        if (read_count > 0U && fwrite(buffer, 1U, read_count, output) != read_count) {
            goto cleanup;
        }
        if (read_count < sizeof(buffer)) {
            if (ferror(input) != 0) {
                goto cleanup;
            }
            break;
        }
    }
    result = 0;

cleanup:
    if (output != NULL && fclose(output) != 0) {
        result = 1;
    }
    if (fclose(input) != 0) {
        result = 1;
    }
    if (result != 0) {
        (void)remove(destination);
    }
    return result;
}

static int open_database(
    enum system_engine_kind kind,
    const char *path,
    struct system_database *out_database
) {
    *out_database = (struct system_database){.kind = kind};
    if (kind == system_engine_mylite) {
        struct mylite_open_diagnostic diagnostic = {0};

        if (mylite_open_with_diagnostic(path, &out_database->mylite, &diagnostic) != MYLITE_OK) {
            fprintf(
                stderr,
                "system-benchmark: MyLite open failed: [%s/%d] %s\n",
                diagnostic.sqlstate,
                diagnostic.error_code,
                diagnostic.message
            );
            return 1;
        }
        if (execute_sql(out_database, "USE perf") != 0) {
            return 1;
        }
        return 0;
    }
    if (sqlite3_open(path, &out_database->sqlite) != SQLITE_OK ||
        sqlite3_busy_timeout(out_database->sqlite, sqlite_busy_timeout_ms) != SQLITE_OK ||
        sqlite3_exec(
            out_database->sqlite,
            "PRAGMA foreign_keys=ON; PRAGMA trusted_schema=OFF",
            NULL,
            NULL,
            NULL
        ) != SQLITE_OK) {
        return 1;
    }
    return 0;
}

static void close_database(struct system_database *database) {
    if (database->mylite != NULL) {
        mylite_close(database->mylite);
        database->mylite = NULL;
    }
    if (database->sqlite != NULL) {
        (void)sqlite3_close(database->sqlite);
        database->sqlite = NULL;
    }
}

static int prepare_statement(
    struct system_database *database,
    const char *sql,
    struct system_statement *out_statement
) {
    *out_statement = (struct system_statement){.kind = database->kind};
    if (database->kind == system_engine_mylite) {
        return mylite_prepare(database->mylite, sql, strlen(sql), &out_statement->mylite) ==
                       MYLITE_OK
                   ? 0
                   : 1;
    }
    return sqlite3_prepare_v2(database->sqlite, sql, -1, &out_statement->sqlite, NULL) == SQLITE_OK
               ? 0
               : 1;
}

static int reset_statement(struct system_statement *statement) {
    if (statement->kind == system_engine_mylite) {
        return mylite_stmt_reset(statement->mylite) == MYLITE_OK ? 0 : 1;
    }
    (void)sqlite3_reset(statement->sqlite);
    return sqlite3_clear_bindings(statement->sqlite) == SQLITE_OK ? 0 : 1;
}

static int step_and_hash(
    struct system_database *database,
    struct system_statement *statement,
    uint64_t *checksum
) {
    if (statement->kind == system_engine_mylite) {
        return step_mylite_and_hash(database, statement, checksum);
    }
    return step_sqlite_and_hash(database, statement, checksum);
}

static int step_mylite_and_hash(
    struct system_database *database,
    struct system_statement *statement,
    uint64_t *checksum
) {
    for (;;) {
        int rc = mylite_stmt_step(statement->mylite);

        if (rc == MYLITE_DONE) {
            return 0;
        }
        if (rc != MYLITE_ROW) {
            fprintf(
                stderr,
                "system-benchmark: MyLite step failed: %s\n",
                mylite_errmsg(database->mylite)
            );
            return 1;
        }
        for (size_t column = 0U; column < mylite_stmt_column_count(statement->mylite); ++column) {
            if (mylite_stmt_value_is_null(statement->mylite, column) != 0) {
                hash_uint64(checksum, UINT64_MAX);
            } else {
                const void *bytes = mylite_stmt_value_bytes(statement->mylite, column);
                size_t size = mylite_stmt_value_size(statement->mylite, column);

                hash_uint64(checksum, size);
                *checksum = hash_bytes(*checksum, bytes, size);
            }
        }
    }
}

static int step_sqlite_and_hash(
    struct system_database *database,
    struct system_statement *statement,
    uint64_t *checksum
) {
    for (;;) {
        int rc = sqlite3_step(statement->sqlite);

        if (rc == SQLITE_DONE) {
            return 0;
        }
        if (rc != SQLITE_ROW) {
            fprintf(
                stderr,
                "system-benchmark: SQLite step failed: %s\n",
                sqlite3_errmsg(database->sqlite)
            );
            return 1;
        }
        for (int column = 0; column < sqlite3_column_count(statement->sqlite); ++column) {
            if (sqlite3_column_type(statement->sqlite, column) == SQLITE_NULL) {
                hash_uint64(checksum, UINT64_MAX);
            } else {
                const void *bytes = sqlite3_column_text(statement->sqlite, column);
                size_t size = (size_t)sqlite3_column_bytes(statement->sqlite, column);

                hash_uint64(checksum, size);
                *checksum = hash_bytes(*checksum, bytes, size);
            }
        }
    }
}

static int finalize_statement(struct system_statement *statement) {
    if (statement->kind == system_engine_mylite) {
        int rc = mylite_stmt_finalize(statement->mylite);

        statement->mylite = NULL;
        return rc == MYLITE_OK ? 0 : 1;
    }
    {
        int rc = sqlite3_finalize(statement->sqlite);

        statement->sqlite = NULL;
        return rc == SQLITE_OK ? 0 : 1;
    }
}

static int execute_sql(struct system_database *database, const char *sql) {
    if (database->kind == system_engine_mylite) {
        mylite_result *result = NULL;
        int rc = mylite_execute(database->mylite, sql, strlen(sql), &result);

        mylite_result_free(result);
        if (rc != MYLITE_OK) {
            fprintf(
                stderr,
                "system-benchmark: MyLite SQL failed: %s\nSQL: %s\n",
                mylite_errmsg(database->mylite),
                sql
            );
            return 1;
        }
        return 0;
    }
    if (sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(
            stderr,
            "system-benchmark: SQLite SQL failed: %s\nSQL: %s\n",
            sqlite3_errmsg(database->sqlite),
            sql
        );
        return 1;
    }
    return 0;
}

static int fetch_count(struct system_database *database, const char *sql, uint64_t *out_count) {
    struct system_statement statement = {0};
    int result = 1;

    if (prepare_statement(database, sql, &statement) != 0 || reset_statement(&statement) != 0) {
        return 1;
    }
    if (database->kind == system_engine_mylite) {
        if (mylite_stmt_step(statement.mylite) != MYLITE_ROW) {
            goto cleanup;
        }
        *out_count = strtoull(mylite_stmt_value_text(statement.mylite, 0U), NULL, decimal_base);
        if (mylite_stmt_step(statement.mylite) != MYLITE_DONE) {
            goto cleanup;
        }
    } else {
        if (sqlite3_step(statement.sqlite) != SQLITE_ROW) {
            goto cleanup;
        }
        *out_count = (uint64_t)sqlite3_column_int64(statement.sqlite, 0);
        if (sqlite3_step(statement.sqlite) != SQLITE_DONE) {
            goto cleanup;
        }
    }
    result = 0;

cleanup:
    if (finalize_statement(&statement) != 0) {
        result = 1;
    }
    return result;
}

static void print_measurement(
    FILE *output,
    const char *scenario,
    size_t row_count,
    enum system_engine_kind kind,
    size_t workers,
    size_t iterations,
    const struct system_measurement *measurement
) {
    double total_ms = (double)measurement->elapsed_ns / (double)nanoseconds_per_millisecond;
    double average_us = measurement->operations == 0U ? 0.0
                                                      : (double)measurement->elapsed_ns /
                                                            (double)nanoseconds_per_microsecond /
                                                            (double)measurement->operations;
    double operations_per_second = measurement->elapsed_ns == 0U
                                       ? 0.0
                                       : (double)measurement->operations *
                                             (double)nanoseconds_per_second /
                                             (double)measurement->elapsed_ns;

    fprintf(
        output,
        "%s,%zu,%s,%zu,%zu,%zu,%zu,%" PRIu64 ",%.3f,%.3f,%.3f,%" PRIu64 ",%" PRIu64 ",%" PRIu64
        "\n",
        scenario,
        row_count,
        engine_name(kind),
        workers,
        iterations,
        measurement->operations,
        measurement->errors,
        measurement->checksum,
        total_ms,
        average_us,
        operations_per_second,
        measurement->bytes_before,
        measurement->bytes_after_delete,
        measurement->bytes_after_reclaim
    );
}

static uint64_t monotonic_now_ns(void) {
#if defined(_WIN32)
    return (uint64_t)clock() * nanoseconds_per_second / (uint64_t)CLOCKS_PER_SEC;
#else
    struct timespec timestamp = {0};

    if (clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0) {
        return 0U;
    }
    return ((uint64_t)timestamp.tv_sec * nanoseconds_per_second) + (uint64_t)timestamp.tv_nsec;
#endif
}

static uint64_t file_size(const char *path) {
#if defined(_WIN32)
    struct _stat64 status = {0};

    return _stat64(path, &status) == 0 ? (uint64_t)status.st_size : 0U;
#else
    struct stat status = {0};

    return stat(path, &status) == 0 ? (uint64_t)status.st_size : 0U;
#endif
}

static uint64_t hash_bytes(uint64_t hash, const void *bytes, size_t size) {
    const unsigned char *data = bytes;

    if (data == NULL) {
        return hash;
    }
    for (size_t index = 0U; index < size; ++index) {
        hash ^= data[index];
        hash *= fnv_prime;
    }
    return hash;
}

static void hash_uint64(uint64_t *hash, uint64_t value) {
    unsigned char bytes[sizeof(value)];

    for (size_t index = 0U; index < sizeof(value); ++index) {
        bytes[index] = (unsigned char)(value >> (index * bits_per_byte));
    }
    *hash = hash_bytes(*hash, bytes, sizeof(bytes));
}

static const char *engine_name(enum system_engine_kind kind) {
    return kind == system_engine_mylite ? "mylite" : "sqlite";
}

static int parse_size(const char *text, size_t *out_value) {
    char *end = NULL;
    unsigned long long value = 0ULL;

    errno = 0;
    value = strtoull(text, &end, decimal_base);
    if (errno != 0 || end == text || *end != '\0' || value == 0ULL || value > SIZE_MAX) {
        return 1;
    }
    *out_value = (size_t)value;
    return 0;
}

static long process_id(void) {
#if defined(_WIN32)
    return (long)_getpid();
#else
    return (long)getpid();
#endif
}
