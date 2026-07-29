#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "sqlite3.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <unistd.h>
#endif

enum {
    qualification_transaction_minimum = 1000,
    default_warmup_transactions = 10,
    maximum_transactions = 1000000,
    maximum_writes_per_transaction = 100,
    generated_sql_capacity = 2048,
    path_capacity = 4096,
    trace_wait_attempts = 6000,
    trace_wait_milliseconds = 10,
    decimal_radix = 10,
    payload_multiplier = 3,
    payload_offset = 7,
    milliseconds_per_second = 1000,
    nanoseconds_per_millisecond = 1000000,
    nanoseconds_per_second = 1000000000ULL,
    percentile_median = 50,
    percentile_p95 = 95,
    percentile_p99 = 99,
    percentile_scale = 100,
    json_control_character_limit = 0x20,
    sqlite_synchronous_extra = 3,
};

enum benchmark_engine {
    benchmark_engine_unknown,
    benchmark_engine_mylite,
    benchmark_engine_sqlite,
};

enum benchmark_statement_shape {
    benchmark_statement_shape_unknown,
    benchmark_statement_shape_repeated,
    benchmark_statement_shape_multi_row,
};

struct benchmark_options {
    enum benchmark_engine engine;
    enum benchmark_statement_shape statement_shape;
    const char *database_path;
    const char *filesystem;
    const char *metadata_output_path;
    const char *output_path;
    const char *ready_file_path;
    const char *revision;
    const char *run_id;
    const char *start_file_path;
    size_t sample;
    size_t transactions;
    size_t warmup_transactions;
    size_t writes_per_transaction;
    bool allow_smoke;
    bool show_help;
};

struct benchmark_database {
    enum benchmark_engine engine;
    mylite_db *mylite;
    sqlite3 *sqlite;
};

struct benchmark_statement {
    enum benchmark_engine engine;
    mylite_stmt *mylite;
    sqlite3_stmt *sqlite;
};

struct path_requirement {
    const char *context;
    const char *path;
};

struct insert_workload {
    enum benchmark_statement_shape shape;
    int64_t first_id;
    size_t writes;
};

struct transaction_measurement {
    uint64_t affected_rows;
    uint64_t elapsed_ns;
};

struct measurement_outputs {
    uint64_t affected_rows;
    uint64_t *latencies;
};

struct benchmark_summary {
    uint64_t maximum_ns;
    uint64_t median_absolute_deviation_ns;
    uint64_t minimum_ns;
    uint64_t p50_ns;
    uint64_t p95_ns;
    uint64_t p99_ns;
    double transactions_per_second;
    double writes_per_second;
};

struct correctness_totals {
    uint64_t id_sum;
    uint64_t payload_sum;
    uint64_t row_count;
};

static int parse_options(int argc, char **argv, struct benchmark_options *out_options);
static int parse_option_value(
    const char *name,
    const char *value,
    struct benchmark_options *options
);
static int parse_size_value(const char *value, size_t *out_value);
static bool valid_label(const char *value);
static void print_usage(const char *program, FILE *stream);
static int run_benchmark(const struct benchmark_options *options);
static int validate_fresh_path(const struct path_requirement *requirement);
static int validate_related_database_paths_fresh(const char *path);
static int open_database(
    const struct benchmark_options *options,
    struct benchmark_database *out_database
);
static int configure_sqlite_baseline(sqlite3 *sqlite);
static int validate_sqlite_baseline(sqlite3 *sqlite);
static int sqlite_pragma_int(sqlite3 *sqlite, const char *sql, int *out_value);
static int sqlite_journal_mode_is_delete(sqlite3 *sqlite);
static int create_schema(struct benchmark_database *database);
static int execute_sql(struct benchmark_database *database, const char *sql);
static int prepare_insert_statement(
    struct benchmark_database *database,
    const struct benchmark_options *options,
    struct benchmark_statement *out_statement
);
static int build_multi_row_insert_sql(char *sql, size_t capacity, size_t writes);
static int prepare_statement(
    struct benchmark_database *database,
    const char *sql,
    struct benchmark_statement *out_statement
);
static int run_warmup(
    struct benchmark_database *database,
    struct benchmark_statement *statement,
    const struct benchmark_options *options
);
static int execute_insert_transaction(
    struct benchmark_database *database,
    struct benchmark_statement *statement,
    const struct insert_workload *workload,
    struct transaction_measurement *out_measurement
);
static int execute_repeated_inserts(
    struct benchmark_statement *statement,
    const struct insert_workload *workload,
    uint64_t *out_affected_rows
);
static int execute_multi_row_insert(
    struct benchmark_statement *statement,
    const struct insert_workload *workload,
    uint64_t *out_affected_rows
);
static int abort_transaction(struct benchmark_database *database, bool explicit_transaction);
static int begin_transaction(struct benchmark_database *database);
static int commit_transaction(struct benchmark_database *database);
static int rollback_transaction(struct benchmark_database *database);
static int reset_statement(struct benchmark_statement *statement);
static int bind_int64(struct benchmark_statement *statement, size_t index, int64_t value);
static int step_write_statement(struct benchmark_statement *statement, uint64_t *out_affected_rows);
static int wait_for_trace_start(const char *ready_path, const char *start_path);
static int create_ready_marker(const char *path);
static bool path_exists(const char *path);
static void wait_milliseconds(unsigned int milliseconds);
static int run_measurements(
    struct benchmark_database *database,
    struct benchmark_statement *statement,
    const struct benchmark_options *options,
    FILE *output,
    struct measurement_outputs *out_measurements
);
static void print_trace_marker(const char *prefix, const struct benchmark_options *options);
static int verify_rows(
    struct benchmark_database *database,
    uint64_t expected_rows,
    struct correctness_totals *out_totals
);
static int query_mylite_totals(mylite_db *database, struct correctness_totals *out_totals);
static int parse_result_uint64(const mylite_result *result, size_t column, uint64_t *out_value);
static int query_sqlite_totals(sqlite3 *sqlite, struct correctness_totals *out_totals);
static int summarize_latencies(
    uint64_t *latencies,
    size_t count,
    size_t writes,
    struct benchmark_summary *out_summary
);
static int compare_uint64(const void *left, const void *right);
static uint64_t percentile(const uint64_t *values, size_t count, size_t percent);
static int write_metadata(
    const struct benchmark_options *options,
    const struct benchmark_database *database,
    const struct benchmark_summary *summary,
    const struct correctness_totals *correctness
);
static void print_json_string(FILE *output, const char *value);
static int finalize_statement(struct benchmark_statement *statement);
static void close_database(struct benchmark_database *database);
static void remove_database_files(const char *path);
static uint64_t monotonic_now_ns(void);
static const char *engine_name(enum benchmark_engine engine);
static const char *statement_shape_name(enum benchmark_statement_shape shape);

int main(int argc, char **argv) {
    struct benchmark_options options = {0};

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

static int parse_options(int argc, char **argv, struct benchmark_options *out_options) {
    struct benchmark_options options = {
        .engine = benchmark_engine_unknown,
        .statement_shape = benchmark_statement_shape_unknown,
        .warmup_transactions = default_warmup_transactions,
    };

    for (int index = 1; index < argc; ++index) {
        const char *argument = argv[index];

        if (strcmp(argument, "--help") == 0) {
            options.show_help = true;
            continue;
        }
        if (strcmp(argument, "--allow-smoke") == 0) {
            options.allow_smoke = true;
            continue;
        }
        if (index + 1 >= argc || parse_option_value(argument, argv[index + 1], &options) != 0) {
            fprintf(stderr, "durable-autocommit: invalid option %s\n", argument);
            return 1;
        }
        ++index;
    }
    if (options.show_help) {
        *out_options = options;
        return 0;
    }
    if (options.engine == benchmark_engine_unknown ||
        options.statement_shape == benchmark_statement_shape_unknown ||
        options.database_path == NULL || options.filesystem == NULL ||
        options.metadata_output_path == NULL || options.output_path == NULL ||
        options.revision == NULL || options.run_id == NULL || options.sample == 0U ||
        options.transactions == 0U || options.writes_per_transaction == 0U) {
        fprintf(stderr, "durable-autocommit: required option is missing\n");
        return 1;
    }
    if ((options.ready_file_path == NULL) != (options.start_file_path == NULL)) {
        fprintf(stderr, "durable-autocommit: ready and start files must be supplied together\n");
        return 1;
    }
    if (!options.allow_smoke && options.transactions < qualification_transaction_minimum) {
        fprintf(
            stderr,
            "durable-autocommit: transaction count is below the qualification minimum\n"
        );
        return 1;
    }
    if (options.transactions > maximum_transactions ||
        (options.writes_per_transaction != 1U && options.writes_per_transaction != 4U &&
         options.writes_per_transaction != maximum_writes_per_transaction) ||
        !valid_label(options.filesystem) || !valid_label(options.revision) ||
        !valid_label(options.run_id)) {
        fprintf(stderr, "durable-autocommit: option value is outside the supported range\n");
        return 1;
    }
    *out_options = options;
    return 0;
}

static int parse_option_value(
    const char *name,
    const char *value,
    struct benchmark_options *options
) {
    if (strcmp(name, "--engine") == 0) {
        if (strcmp(value, "mylite") == 0) {
            options->engine = benchmark_engine_mylite;
        } else if (strcmp(value, "sqlite") == 0) {
            options->engine = benchmark_engine_sqlite;
        } else {
            return 1;
        }
        return 0;
    }
    if (strcmp(name, "--statement-shape") == 0) {
        if (strcmp(value, "repeated") == 0) {
            options->statement_shape = benchmark_statement_shape_repeated;
        } else if (strcmp(value, "multi-row") == 0) {
            options->statement_shape = benchmark_statement_shape_multi_row;
        } else {
            return 1;
        }
        return 0;
    }
    if (strcmp(name, "--database") == 0) {
        options->database_path = value;
    } else if (strcmp(name, "--filesystem") == 0) {
        options->filesystem = value;
    } else if (strcmp(name, "--metadata-output") == 0) {
        options->metadata_output_path = value;
    } else if (strcmp(name, "--output") == 0) {
        options->output_path = value;
    } else if (strcmp(name, "--ready-file") == 0) {
        options->ready_file_path = value;
    } else if (strcmp(name, "--revision") == 0) {
        options->revision = value;
    } else if (strcmp(name, "--run-id") == 0) {
        options->run_id = value;
    } else if (strcmp(name, "--start-file") == 0) {
        options->start_file_path = value;
    } else if (strcmp(name, "--sample") == 0) {
        return parse_size_value(value, &options->sample);
    } else if (strcmp(name, "--transactions") == 0) {
        return parse_size_value(value, &options->transactions);
    } else if (strcmp(name, "--warmup") == 0) {
        return parse_size_value(value, &options->warmup_transactions);
    } else if (strcmp(name, "--writes-per-transaction") == 0) {
        return parse_size_value(value, &options->writes_per_transaction);
    } else {
        return 1;
    }
    return value[0] == '\0' ? 1 : 0;
}

static int parse_size_value(const char *value, size_t *out_value) {
    char *end = NULL;
    unsigned long long parsed = 0U;

    if (value == NULL || value[0] == '\0' || value[0] == '-') {
        return 1;
    }
    errno = 0;
    parsed = strtoull(value, &end, decimal_radix);
    if (errno != 0 || end == value || *end != '\0' || parsed > SIZE_MAX) {
        return 1;
    }
    *out_value = (size_t)parsed;
    return 0;
}

static bool valid_label(const char *value) {
    if (value == NULL || value[0] == '\0') {
        return false;
    }
    for (const unsigned char *cursor = (const unsigned char *)value; *cursor != '\0'; ++cursor) {
        if (!((*cursor >= 'a' && *cursor <= 'z') || (*cursor >= 'A' && *cursor <= 'Z') ||
              (*cursor >= '0' && *cursor <= '9') || *cursor == '_' || *cursor == '.' ||
              *cursor == ':' || *cursor == '+' || *cursor == '-')) {
            return false;
        }
    }
    return true;
}

static void print_usage(const char *program, FILE *stream) {
    fprintf(
        stream,
        "usage: %s --engine mylite|sqlite --database PATH --filesystem FSTYPE\\\n"
        "  --output PATH --metadata-output PATH --revision REVISION --run-id ID\\\n"
        "  --sample N --statement-shape repeated|multi-row --transactions N\\\n"
        "  --writes-per-transaction 1|4|100 [--warmup N] [--allow-smoke]\\\n"
        "  [--ready-file PATH --start-file PATH]\n",
        program
    );
}

static int run_benchmark(const struct benchmark_options *options) {
    struct benchmark_database database = {0};
    struct benchmark_statement statement = {0};
    struct benchmark_summary summary = {0};
    struct correctness_totals correctness = {0};
    FILE *output = NULL;
    uint64_t *latencies = NULL;
    struct measurement_outputs measurements = {0};
    uint64_t expected_rows =
        (uint64_t)options->transactions * (uint64_t)options->writes_per_transaction;
    int result = 1;

    if (validate_fresh_path(&(struct path_requirement){
            .context = "database",
            .path = options->database_path,
        }) != 0 ||
        validate_related_database_paths_fresh(options->database_path) != 0 ||
        validate_fresh_path(&(struct path_requirement){
            .context = "raw output",
            .path = options->output_path,
        }) != 0 ||
        validate_fresh_path(&(struct path_requirement){
            .context = "metadata output",
            .path = options->metadata_output_path,
        }) != 0 ||
        (options->ready_file_path != NULL && (validate_fresh_path(&(struct path_requirement){
                                                  .context = "ready file",
                                                  .path = options->ready_file_path,
                                              }) != 0 ||
                                              validate_fresh_path(&(struct path_requirement){
                                                  .context = "start file",
                                                  .path = options->start_file_path,
                                              }) != 0))) {
        goto cleanup;
    }
    if (open_database(options, &database) != 0 || create_schema(&database) != 0 ||
        prepare_insert_statement(&database, options, &statement) != 0 ||
        run_warmup(&database, &statement, options) != 0 ||
        wait_for_trace_start(options->ready_file_path, options->start_file_path) != 0) {
        goto cleanup;
    }
    output = fopen(options->output_path, "wx");
    latencies = calloc(options->transactions, sizeof(*latencies));
    measurements.latencies = latencies;
    if (output == NULL || latencies == NULL) {
        fprintf(stderr, "durable-autocommit: failed to create measurement outputs\n");
        goto cleanup;
    }
    fprintf(
        output,
        "run_id,revision,engine,filesystem,statement_shape,writes_per_transaction,sample,"
        "transaction_index,elapsed_ns,affected_rows,cumulative_rows,status\n"
    );
    if (run_measurements(&database, &statement, options, output, &measurements) != 0 ||
        fflush(output) != 0 || measurements.affected_rows != expected_rows ||
        verify_rows(&database, expected_rows, &correctness) != 0) {
        goto cleanup;
    }
    if (summarize_latencies(
            latencies,
            options->transactions,
            options->writes_per_transaction,
            &summary
        ) != 0 ||
        write_metadata(options, &database, &summary, &correctness) != 0) {
        goto cleanup;
    }
    result = 0;

cleanup:
    if (output != NULL && fclose(output) != 0) {
        result = 1;
    }
    if ((statement.mylite != NULL || statement.sqlite != NULL) &&
        finalize_statement(&statement) != 0) {
        result = 1;
    }
    free(latencies);
    close_database(&database);
    remove_database_files(options->database_path);
    if (options->ready_file_path != NULL) {
        (void)remove(options->ready_file_path);
    }
    return result;
}

static int validate_fresh_path(const struct path_requirement *requirement) {
    const char *separator = NULL;
    char parent[path_capacity];
    struct stat status = {0};
    size_t parent_length = 0U;

    if (requirement->path == NULL || requirement->path[0] == '\0' || requirement->path[0] != '/' ||
        stat(requirement->path, &status) == 0) {
        fprintf(
            stderr,
            "durable-autocommit: %s path is invalid or already exists\n",
            requirement->context
        );
        return 1;
    }
    separator = strrchr(requirement->path, '/');
    if (separator == NULL || separator == requirement->path || separator[1] == '\0') {
        fprintf(stderr, "durable-autocommit: %s parent path is invalid\n", requirement->context);
        return 1;
    }
    parent_length = (size_t)(separator - requirement->path);
    if (parent_length >= sizeof(parent)) {
        return 1;
    }
    memcpy(parent, requirement->path, parent_length);
    parent[parent_length] = '\0';
    if (stat(parent, &status) != 0 || !S_ISDIR(status.st_mode)) {
        fprintf(
            stderr,
            "durable-autocommit: %s parent directory does not exist\n",
            requirement->context
        );
        return 1;
    }
    return 0;
}

static int validate_related_database_paths_fresh(const char *path) {
    static const char *const suffixes[] = {"-journal", "-wal", "-shm"};
    char related_path[path_capacity];
    struct stat status = {0};

    for (size_t index = 0U; index < sizeof(suffixes) / sizeof(suffixes[0]); ++index) {
        int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffixes[index]);

        if (written < 0 || (size_t)written >= sizeof(related_path) ||
            stat(related_path, &status) == 0) {
            fprintf(
                stderr,
                "durable-autocommit: related database path is invalid or already exists\n"
            );
            return 1;
        }
    }
    return 0;
}

static int open_database(
    const struct benchmark_options *options,
    struct benchmark_database *out_database
) {
    int rc = 0;

    *out_database = (struct benchmark_database){.engine = options->engine};
    if (options->engine == benchmark_engine_mylite) {
        rc = mylite_open(options->database_path, &out_database->mylite);
        if (rc != MYLITE_OK) {
            fprintf(stderr, "durable-autocommit: MyLite open failed: %d\n", rc);
            return 1;
        }
        if (validate_sqlite_baseline(mylite_connection_sqlite_for_test(out_database->mylite)) !=
            0) {
            fprintf(stderr, "durable-autocommit: MyLite durability readback failed\n");
            return 1;
        }
        return 0;
    }
    rc = sqlite3_open_v2(
        options->database_path,
        &out_database->sqlite,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_EXCLUSIVE,
        NULL
    );
    if (rc != SQLITE_OK || configure_sqlite_baseline(out_database->sqlite) != 0) {
        fprintf(
            stderr,
            "durable-autocommit: SQLite open/configuration failed: %s\n",
            out_database->sqlite == NULL ? "no connection" : sqlite3_errmsg(out_database->sqlite)
        );
        return 1;
    }
    return 0;
}

static int configure_sqlite_baseline(sqlite3 *sqlite) {
    if (sqlite == NULL) {
        return 1;
    }
    if (sqlite3_exec(
            sqlite,
            "PRAGMA journal_mode=DELETE; PRAGMA synchronous=EXTRA; PRAGMA mmap_size=0",
            NULL,
            NULL,
            NULL
        ) != SQLITE_OK) {
        return 1;
    }
    return validate_sqlite_baseline(sqlite);
}

static int validate_sqlite_baseline(sqlite3 *sqlite) {
    if (sqlite == NULL || sqlite_journal_mode_is_delete(sqlite) != 0) {
        return 1;
    }
    {
        int synchronous = 0;
        int mmap_size = -1;

        return sqlite_pragma_int(sqlite, "PRAGMA synchronous", &synchronous) != 0 ||
                       sqlite_pragma_int(sqlite, "PRAGMA mmap_size", &mmap_size) != 0 ||
                       synchronous != sqlite_synchronous_extra || mmap_size != 0
                   ? 1
                   : 0;
    }
}

static int sqlite_pragma_int(sqlite3 *sqlite, const char *sql, int *out_value) {
    sqlite3_stmt *statement = NULL;
    int result = 1;

    if (sqlite3_prepare_v2(sqlite, sql, -1, &statement, NULL) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW) {
        *out_value = sqlite3_column_int(statement, 0);
        result = 0;
    }
    (void)sqlite3_finalize(statement);
    return result;
}

static int sqlite_journal_mode_is_delete(sqlite3 *sqlite) {
    sqlite3_stmt *statement = NULL;
    int result = 1;

    if (sqlite3_prepare_v2(sqlite, "PRAGMA journal_mode", -1, &statement, NULL) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW) {
        const unsigned char *value = sqlite3_column_text(statement, 0);

        result = value != NULL && strcmp((const char *)value, "delete") == 0 ? 0 : 1;
    }
    (void)sqlite3_finalize(statement);
    return result;
}

static int create_schema(struct benchmark_database *database) {
    if (database->engine == benchmark_engine_mylite) {
        return execute_sql(database, "CREATE DATABASE durable") != 0 ||
                       execute_sql(database, "USE durable") != 0 ||
                       execute_sql(
                           database,
                           "CREATE TABLE durable_rows ("
                           "id BIGINT PRIMARY KEY, payload BIGINT NOT NULL)"
                       ) != 0
                   ? 1
                   : 0;
    }
    return execute_sql(
        database,
        "CREATE TABLE durable_rows (id INTEGER PRIMARY KEY, payload INTEGER NOT NULL)"
    );
}

static int execute_sql(struct benchmark_database *database, const char *sql) {
    if (database->engine == benchmark_engine_mylite) {
        mylite_result *result = NULL;
        int rc = mylite_execute(database->mylite, sql, strlen(sql), &result);

        mylite_result_free(result);
        if (rc != MYLITE_OK) {
            fprintf(
                stderr,
                "durable-autocommit: MyLite SQL failed: %s\n",
                mylite_errmsg(database->mylite)
            );
            return 1;
        }
        return 0;
    }
    if (sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(
            stderr,
            "durable-autocommit: SQLite SQL failed: %s\n",
            sqlite3_errmsg(database->sqlite)
        );
        return 1;
    }
    return 0;
}

static int prepare_insert_statement(
    struct benchmark_database *database,
    const struct benchmark_options *options,
    struct benchmark_statement *out_statement
) {
    char sql[generated_sql_capacity];

    if (options->statement_shape == benchmark_statement_shape_repeated) {
        return prepare_statement(
            database,
            "INSERT INTO durable_rows (id, payload) VALUES (?, ?)",
            out_statement
        );
    }
    if (build_multi_row_insert_sql(sql, sizeof(sql), options->writes_per_transaction) != 0) {
        return 1;
    }
    return prepare_statement(database, sql, out_statement);
}

static int build_multi_row_insert_sql(char *sql, size_t capacity, size_t writes) {
    static const char prefix[] = "INSERT INTO durable_rows (id, payload) VALUES ";
    size_t used = sizeof(prefix) - 1U;

    if (writes == 0U || used >= capacity) {
        return 1;
    }
    memcpy(sql, prefix, used);
    for (size_t index = 0U; index < writes; ++index) {
        static const char tuple[] = "(?, ?)";
        const char *separator = index == 0U ? "" : ", ";
        size_t separator_length = strlen(separator);

        if (used + separator_length + sizeof(tuple) > capacity) {
            return 1;
        }
        memcpy(sql + used, separator, separator_length);
        used += separator_length;
        memcpy(sql + used, tuple, sizeof(tuple) - 1U);
        used += sizeof(tuple) - 1U;
    }
    sql[used] = '\0';
    return 0;
}

static int prepare_statement(
    struct benchmark_database *database,
    const char *sql,
    struct benchmark_statement *out_statement
) {
    *out_statement = (struct benchmark_statement){.engine = database->engine};
    if (database->engine == benchmark_engine_mylite) {
        if (mylite_prepare(database->mylite, sql, strlen(sql), &out_statement->mylite) !=
            MYLITE_OK) {
            fprintf(
                stderr,
                "durable-autocommit: MyLite prepare failed: %s\n",
                mylite_errmsg(database->mylite)
            );
            return 1;
        }
        return 0;
    }
    if (sqlite3_prepare_v2(database->sqlite, sql, -1, &out_statement->sqlite, NULL) != SQLITE_OK) {
        fprintf(
            stderr,
            "durable-autocommit: SQLite prepare failed: %s\n",
            sqlite3_errmsg(database->sqlite)
        );
        return 1;
    }
    return 0;
}

static int run_warmup(
    struct benchmark_database *database,
    struct benchmark_statement *statement,
    const struct benchmark_options *options
) {
    int64_t next_id = -(int64_t)(options->warmup_transactions * options->writes_per_transaction);

    for (size_t index = 0U; index < options->warmup_transactions; ++index) {
        struct insert_workload workload = {
            .shape = options->statement_shape,
            .first_id = next_id,
            .writes = options->writes_per_transaction,
        };
        struct transaction_measurement measurement = {0};

        if (execute_insert_transaction(database, statement, &workload, &measurement) != 0 ||
            measurement.affected_rows != options->writes_per_transaction) {
            return 1;
        }
        next_id += (int64_t)options->writes_per_transaction;
    }
    return options->warmup_transactions == 0U ? 0
                                              : execute_sql(database, "DELETE FROM durable_rows");
}

static int execute_insert_transaction(
    struct benchmark_database *database,
    struct benchmark_statement *statement,
    const struct insert_workload *workload,
    struct transaction_measurement *out_measurement
) {
    bool explicit_transaction = workload->writes > 1U;
    uint64_t started = monotonic_now_ns();
    uint64_t affected_rows = 0U;
    int rc = 0;

    *out_measurement = (struct transaction_measurement){0};
    if (explicit_transaction && begin_transaction(database) != 0) {
        return 1;
    }
    rc = workload->shape == benchmark_statement_shape_repeated
             ? execute_repeated_inserts(statement, workload, &affected_rows)
             : execute_multi_row_insert(statement, workload, &affected_rows);
    if (rc != 0) {
        return abort_transaction(database, explicit_transaction);
    }
    if (explicit_transaction && commit_transaction(database) != 0) {
        return 1;
    }
    out_measurement->elapsed_ns = monotonic_now_ns() - started;
    out_measurement->affected_rows = affected_rows;
    return 0;
}

static int execute_repeated_inserts(
    struct benchmark_statement *statement,
    const struct insert_workload *workload,
    uint64_t *out_affected_rows
) {
    uint64_t affected_rows = 0U;

    for (size_t index = 0U; index < workload->writes; ++index) {
        int64_t id = workload->first_id + (int64_t)index;
        uint64_t statement_affected_rows = 0U;

        if (reset_statement(statement) != 0 || bind_int64(statement, 0U, id) != 0 ||
            bind_int64(statement, 1U, (id * payload_multiplier) + payload_offset) != 0 ||
            step_write_statement(statement, &statement_affected_rows) != 0) {
            return 1;
        }
        affected_rows += statement_affected_rows;
    }
    *out_affected_rows = affected_rows;
    return 0;
}

static int execute_multi_row_insert(
    struct benchmark_statement *statement,
    const struct insert_workload *workload,
    uint64_t *out_affected_rows
) {
    if (reset_statement(statement) != 0) {
        return 1;
    }
    for (size_t index = 0U; index < workload->writes; ++index) {
        int64_t id = workload->first_id + (int64_t)index;

        if (bind_int64(statement, index * 2U, id) != 0 ||
            bind_int64(statement, (index * 2U) + 1U, (id * payload_multiplier) + payload_offset) !=
                0) {
            return 1;
        }
    }
    return step_write_statement(statement, out_affected_rows);
}

static int abort_transaction(struct benchmark_database *database, bool explicit_transaction) {
    if (explicit_transaction) {
        (void)rollback_transaction(database);
    }
    return 1;
}

static int begin_transaction(struct benchmark_database *database) {
    return execute_sql(
        database,
        database->engine == benchmark_engine_mylite ? "START TRANSACTION" : "BEGIN"
    );
}

static int commit_transaction(struct benchmark_database *database) {
    return execute_sql(database, "COMMIT");
}

static int rollback_transaction(struct benchmark_database *database) {
    return execute_sql(database, "ROLLBACK");
}

static int reset_statement(struct benchmark_statement *statement) {
    if (statement->engine == benchmark_engine_mylite) {
        return mylite_stmt_reset(statement->mylite) == MYLITE_OK ? 0 : 1;
    }
    (void)sqlite3_reset(statement->sqlite);
    return sqlite3_clear_bindings(statement->sqlite) == SQLITE_OK ? 0 : 1;
}

static int bind_int64(struct benchmark_statement *statement, size_t index, int64_t value) {
    if (statement->engine == benchmark_engine_mylite) {
        return mylite_stmt_bind_int64(statement->mylite, index, value) == MYLITE_OK ? 0 : 1;
    }
    return sqlite3_bind_int64(statement->sqlite, (int)index + 1, value) == SQLITE_OK ? 0 : 1;
}

static int step_write_statement(
    struct benchmark_statement *statement,
    uint64_t *out_affected_rows
) {
    if (statement->engine == benchmark_engine_mylite) {
        if (mylite_stmt_step(statement->mylite) != MYLITE_DONE) {
            return 1;
        }
        *out_affected_rows = (uint64_t)mylite_stmt_affected_rows(statement->mylite);
        return 0;
    }
    if (sqlite3_step(statement->sqlite) != SQLITE_DONE) {
        return 1;
    }
    *out_affected_rows = (uint64_t)sqlite3_changes64(sqlite3_db_handle(statement->sqlite));
    return 0;
}

static int wait_for_trace_start(const char *ready_path, const char *start_path) {
    if (ready_path == NULL || start_path == NULL) {
        return 0;
    }
    if (create_ready_marker(ready_path) != 0) {
        return 1;
    }
    for (size_t attempt = 0U; attempt < trace_wait_attempts; ++attempt) {
        if (path_exists(start_path)) {
            return 0;
        }
        wait_milliseconds(trace_wait_milliseconds);
    }
    fprintf(stderr, "durable-autocommit: timed out waiting for trace start\n");
    return 1;
}

static int create_ready_marker(const char *path) {
    FILE *file = fopen(path, "wx");
    int result = 0;

    if (file == NULL) {
        fprintf(stderr, "durable-autocommit: failed to create trace marker\n");
        return 1;
    }
    if (fputs("ready\n", file) == EOF || fclose(file) != 0) {
        result = 1;
    }
    return result;
}

static bool path_exists(const char *path) {
    struct stat status = {0};

    return path != NULL && stat(path, &status) == 0 && S_ISREG(status.st_mode);
}

static void wait_milliseconds(unsigned int milliseconds) {
#if defined(_WIN32)
    Sleep(milliseconds);
#else
    struct timespec duration = {
        .tv_sec = milliseconds / milliseconds_per_second,
        .tv_nsec = (long)(milliseconds % milliseconds_per_second) * nanoseconds_per_millisecond,
    };

    while (nanosleep(&duration, &duration) != 0 && errno == EINTR) {}
#endif
}

static int run_measurements(
    struct benchmark_database *database,
    struct benchmark_statement *statement,
    const struct benchmark_options *options,
    FILE *output,
    struct measurement_outputs *out_measurements
) {
    int64_t next_id = 1;
    uint64_t cumulative_rows = 0U;

    print_trace_marker("MYLITE_DURABLE_TRACE_BEGIN", options);
    for (size_t transaction = 0U; transaction < options->transactions; ++transaction) {
        struct insert_workload workload = {
            .shape = options->statement_shape,
            .first_id = next_id,
            .writes = options->writes_per_transaction,
        };
        struct transaction_measurement measurement = {0};

        if (execute_insert_transaction(database, statement, &workload, &measurement) != 0) {
            fprintf(stderr, "durable-autocommit: measured transaction failed\n");
            return 1;
        }
        out_measurements->latencies[transaction] = measurement.elapsed_ns;
        cumulative_rows += measurement.affected_rows;
        fprintf(
            output,
            "%s,%s,%s,%s,%s,%zu,%zu,%zu,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",ok\n",
            options->run_id,
            options->revision,
            engine_name(options->engine),
            options->filesystem,
            statement_shape_name(options->statement_shape),
            options->writes_per_transaction,
            options->sample,
            transaction + 1U,
            measurement.elapsed_ns,
            measurement.affected_rows,
            cumulative_rows
        );
        next_id += (int64_t)options->writes_per_transaction;
    }
    if (fflush(output) != 0) {
        return 1;
    }
    print_trace_marker("MYLITE_DURABLE_TRACE_END", options);
    out_measurements->affected_rows = cumulative_rows;
    return 0;
}

static void print_trace_marker(const char *prefix, const struct benchmark_options *options) {
    fprintf(
        stderr,
        "%s %s %s %s %zu %zu\n",
        prefix,
        options->run_id,
        engine_name(options->engine),
        statement_shape_name(options->statement_shape),
        options->writes_per_transaction,
        options->sample
    );
    (void)fflush(stderr);
}

static int verify_rows(
    struct benchmark_database *database,
    uint64_t expected_rows,
    struct correctness_totals *out_totals
) {
    uint64_t expected_id_sum = expected_rows * (expected_rows + 1U) / 2U;
    uint64_t expected_payload_sum =
        (payload_multiplier * expected_id_sum) + (payload_offset * expected_rows);
    int result = database->engine == benchmark_engine_mylite
                     ? query_mylite_totals(database->mylite, out_totals)
                     : query_sqlite_totals(database->sqlite, out_totals);

    if (result != 0 || out_totals->row_count != expected_rows ||
        out_totals->id_sum != expected_id_sum || out_totals->payload_sum != expected_payload_sum) {
        fprintf(
            stderr,
            "durable-autocommit: row-count or checksum verification failed "
            "(rows=%" PRIu64 "/%" PRIu64 ", id-sum=%" PRIu64 "/%" PRIu64 ", payload-sum=%" PRIu64
            "/%" PRIu64 ")\n",
            out_totals->row_count,
            expected_rows,
            out_totals->id_sum,
            expected_id_sum,
            out_totals->payload_sum,
            expected_payload_sum
        );
        return 1;
    }
    return 0;
}

static int query_mylite_totals(mylite_db *database, struct correctness_totals *out_totals) {
    static const char sql[] = "SELECT COUNT(*), SUM(id), SUM(payload) FROM durable_rows";
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, sizeof(sql) - 1U, &result);
    int status = 1;

    if (rc == MYLITE_OK && result != NULL && mylite_result_row_count(result) == 1U &&
        parse_result_uint64(result, 0U, &out_totals->row_count) == 0 &&
        parse_result_uint64(result, 1U, &out_totals->id_sum) == 0 &&
        parse_result_uint64(result, 2U, &out_totals->payload_sum) == 0) {
        status = 0;
    }
    if (status != 0) {
        fprintf(
            stderr,
            "durable-autocommit: MyLite verification query failed: rc=%d rows=%zu columns=%zu "
            "error=%s\n",
            rc,
            result == NULL ? 0U : mylite_result_row_count(result),
            result == NULL ? 0U : mylite_result_column_count(result),
            mylite_errmsg(database)
        );
    }
    mylite_result_free(result);
    return status;
}

static int parse_result_uint64(const mylite_result *result, size_t column, uint64_t *out_value) {
    const char *text = mylite_result_value_text(result, 0U, column);
    char *end = NULL;
    unsigned long long value = 0U;

    if (text == NULL) {
        return 1;
    }
    errno = 0;
    value = strtoull(text, &end, decimal_radix);
    if (errno != 0 || end == text || *end != '\0') {
        return 1;
    }
    *out_value = (uint64_t)value;
    return 0;
}

static int query_sqlite_totals(sqlite3 *sqlite, struct correctness_totals *out_totals) {
    static const char sql[] = "SELECT COUNT(*), SUM(id), SUM(payload) FROM durable_rows";
    sqlite3_stmt *statement = NULL;
    int result = 1;

    if (sqlite3_prepare_v2(sqlite, sql, -1, &statement, NULL) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW) {
        out_totals->row_count = (uint64_t)sqlite3_column_int64(statement, 0);
        out_totals->id_sum = (uint64_t)sqlite3_column_int64(statement, 1);
        out_totals->payload_sum = (uint64_t)sqlite3_column_int64(statement, 2);
        result = 0;
    }
    (void)sqlite3_finalize(statement);
    return result;
}

static int summarize_latencies(
    uint64_t *latencies,
    size_t count,
    size_t writes,
    struct benchmark_summary *out_summary
) {
    uint64_t *deviations = calloc(count, sizeof(*deviations));
    uint64_t total_ns = 0U;

    if (deviations == NULL) {
        fprintf(stderr, "durable-autocommit: failed to allocate summary workspace\n");
        return 1;
    }
    qsort(latencies, count, sizeof(*latencies), compare_uint64);
    out_summary->minimum_ns = latencies[0];
    out_summary->maximum_ns = latencies[count - 1U];
    out_summary->p50_ns = percentile(latencies, count, percentile_median);
    out_summary->p95_ns = percentile(latencies, count, percentile_p95);
    out_summary->p99_ns = percentile(latencies, count, percentile_p99);
    for (size_t index = 0U; index < count; ++index) {
        total_ns += latencies[index];
        deviations[index] = latencies[index] > out_summary->p50_ns
                                ? latencies[index] - out_summary->p50_ns
                                : out_summary->p50_ns - latencies[index];
    }
    qsort(deviations, count, sizeof(*deviations), compare_uint64);
    out_summary->median_absolute_deviation_ns = percentile(deviations, count, percentile_median);
    free(deviations);
    if (total_ns != 0U) {
        out_summary->transactions_per_second =
            (double)count * (double)nanoseconds_per_second / (double)total_ns;
        out_summary->writes_per_second =
            (double)(count * writes) * (double)nanoseconds_per_second / (double)total_ns;
    }
    return 0;
}

static int compare_uint64(
    const void *left, // NOLINT(bugprone-easily-swappable-parameters): qsort comparator ABI.
    const void *right
) {
    uint64_t left_value = *(const uint64_t *)left;
    uint64_t right_value = *(const uint64_t *)right;

    if (left_value < right_value) {
        return -1;
    }
    return left_value > right_value ? 1 : 0;
}

static uint64_t percentile(const uint64_t *values, size_t count, size_t percent) {
    size_t rank = ((percent * count) + percentile_scale - 1U) / percentile_scale;

    return values[rank == 0U ? 0U : rank - 1U];
}

static int write_metadata(
    const struct benchmark_options *options,
    const struct benchmark_database *database,
    const struct benchmark_summary *summary,
    const struct correctness_totals *correctness
) {
    FILE *output = fopen(options->metadata_output_path, "wx");
    int result = 0;

    if (output == NULL) {
        fprintf(stderr, "durable-autocommit: failed to create metadata output\n");
        return 1;
    }
    fputs("{\n  \"configuration\": {\n    \"database_path\": ", output);
    print_json_string(output, options->database_path);
    fprintf(
        output,
        ",\n    \"journal_mode\": \"delete\",\n"
        "    \"mmap_size\": 0,\n"
        "    \"sqlite_version\": \"%s\",\n"
        "    \"synchronous\": 3,\n"
        "    \"configuration_source\": \"%s\"\n"
        "  },\n"
        "  \"correctness\": {\n"
        "    \"id_sum\": %" PRIu64 ",\n"
        "    \"payload_sum\": %" PRIu64 ",\n"
        "    \"row_count\": %" PRIu64 "\n"
        "  },\n"
        "  \"engine\": \"%s\",\n"
        "  \"filesystem\": \"%s\",\n"
        "  \"revision\": \"%s\",\n"
        "  \"run_id\": \"%s\",\n"
        "  \"sample\": %zu,\n"
        "  \"statement_shape\": \"%s\",\n"
        "  \"summary\": {\n"
        "    \"maximum_ns\": %" PRIu64 ",\n"
        "    \"median_absolute_deviation_ns\": %" PRIu64 ",\n"
        "    \"minimum_ns\": %" PRIu64 ",\n"
        "    \"p50_ns\": %" PRIu64 ",\n"
        "    \"p95_ns\": %" PRIu64 ",\n"
        "    \"p99_ns\": %" PRIu64 ",\n"
        "    \"transaction_count\": %zu,\n"
        "    \"transactions_per_second\": %.6f,\n"
        "    \"writes_per_second\": %.6f\n"
        "  },\n"
        "  \"transactions\": %zu,\n"
        "  \"warmup_transactions\": %zu,\n"
        "  \"writes_per_transaction\": %zu\n"
        "}\n",
        sqlite3_libversion(),
        database->engine == benchmark_engine_mylite ? "MyLite file-backed storage readback"
                                                    : "benchmark readback",
        correctness->id_sum,
        correctness->payload_sum,
        correctness->row_count,
        engine_name(options->engine),
        options->filesystem,
        options->revision,
        options->run_id,
        options->sample,
        statement_shape_name(options->statement_shape),
        summary->maximum_ns,
        summary->median_absolute_deviation_ns,
        summary->minimum_ns,
        summary->p50_ns,
        summary->p95_ns,
        summary->p99_ns,
        options->transactions,
        summary->transactions_per_second,
        summary->writes_per_second,
        options->transactions,
        options->warmup_transactions,
        options->writes_per_transaction
    );
    if (ferror(output) || fclose(output) != 0) {
        result = 1;
    }
    return result;
}

static void print_json_string(FILE *output, const char *value) {
    fputc('"', output);
    for (const unsigned char *cursor = (const unsigned char *)value; *cursor != '\0'; ++cursor) {
        switch (*cursor) {
        case '"':
            fputs("\\\"", output);
            break;
        case '\\':
            fputs("\\\\", output);
            break;
        case '\b':
            fputs("\\b", output);
            break;
        case '\f':
            fputs("\\f", output);
            break;
        case '\n':
            fputs("\\n", output);
            break;
        case '\r':
            fputs("\\r", output);
            break;
        case '\t':
            fputs("\\t", output);
            break;
        default:
            if (*cursor < json_control_character_limit) {
                fprintf(output, "\\u%04x", *cursor);
            } else {
                fputc(*cursor, output);
            }
            break;
        }
    }
    fputc('"', output);
}

static int finalize_statement(struct benchmark_statement *statement) {
    if (statement->engine == benchmark_engine_mylite) {
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

static void close_database(struct benchmark_database *database) {
    if (database->mylite != NULL) {
        mylite_close(database->mylite);
        database->mylite = NULL;
    }
    if (database->sqlite != NULL) {
        (void)sqlite3_close(database->sqlite);
        database->sqlite = NULL;
    }
}

static void remove_database_files(const char *path) {
    static const char *const suffixes[] = {"", "-journal", "-wal", "-shm"};
    char related[path_capacity];

    if (path == NULL || path[0] == '\0') {
        return;
    }
    for (size_t index = 0U; index < sizeof(suffixes) / sizeof(suffixes[0]); ++index) {
        int written = snprintf(related, sizeof(related), "%s%s", path, suffixes[index]);

        if (written >= 0 && (size_t)written < sizeof(related)) {
            (void)remove(related);
        }
    }
}

static uint64_t monotonic_now_ns(void) {
#if defined(_WIN32)
    LARGE_INTEGER frequency = {0};
    LARGE_INTEGER counter = {0};

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)counter.QuadPart * nanoseconds_per_second / (uint64_t)frequency.QuadPart;
#else
    struct timespec timestamp = {0};

    if (clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0) {
        return 0U;
    }
    return ((uint64_t)timestamp.tv_sec * nanoseconds_per_second) + (uint64_t)timestamp.tv_nsec;
#endif
}

static const char *engine_name(enum benchmark_engine engine) {
    return engine == benchmark_engine_mylite ? "mylite" : "sqlite";
}

static const char *statement_shape_name(enum benchmark_statement_shape shape) {
    return shape == benchmark_statement_shape_repeated ? "repeated" : "multi-row";
}
