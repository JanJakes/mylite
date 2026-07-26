#include <mylite/mylite.h>

#include "sqlite3.h"

#ifdef MYLITE_ENABLE_PROFILING
#  include "runtime/mylite_profile_internal.h"
#endif

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
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    default_row_count = 100000,
    default_sample_count = 5,
    default_warmup_iterations = 1,
    path_capacity = 1024,
    generated_text_capacity = 160,
    decimal_base = 10,
    sqlite_busy_timeout_ms = 5000,
    nanoseconds_per_second = 1000000000ULL,
    nanoseconds_per_millisecond = 1000000,
    nanoseconds_per_microsecond = 1000,
    microseconds_per_second = 1000000ULL,
    milliseconds_per_second = 1000,
    minimum_account_count = 100,
    minimum_tag_count = 100,
    maximum_tag_count = 2000,
    rows_per_account = 100,
    account_region_count = 32,
    category_count = 1000,
    score_multiplier = 48271,
    score_modulus = 100000,
    score_range_lower = 10000,
    score_range_upper = 90000,
    correlated_score_threshold = 95000,
    pseudo_random_increment = 17,
    bridge_tag_span = 9,
    write_log_id_factor = 10,
    seed_progress_interval = 100000,
    seed_timestamp_base = 1700000000,
    seconds_per_year = 31536000,
    tag_weight_modulus = 100,
    item_created_at_parameter = 5,
    item_title_parameter = 6,
    item_payload_parameter = 7,
    bits_per_byte = 8,
    byte_mask = 255,
    median_pair_count = 2,
};

static const uint64_t fnv_offset_basis = 1469598103934665603ULL;

enum benchmark_engine_kind {
    benchmark_engine_mylite,
    benchmark_engine_sqlite,
};

enum benchmark_execution_mode {
    benchmark_execution_prepare_each,
    benchmark_execution_prepared,
    benchmark_execution_write_rollback,
};

enum benchmark_scenario_id {
    benchmark_scenario_point_lookup_prepare_each,
    benchmark_scenario_point_lookup_prepared,
    benchmark_scenario_secondary_lookup,
    benchmark_scenario_range_aggregate,
    benchmark_scenario_full_scan_expression,
    benchmark_scenario_text_expression,
    benchmark_scenario_group_aggregate,
    benchmark_scenario_indexed_order_limit,
    benchmark_scenario_parent_join,
    benchmark_scenario_bridge_join,
    benchmark_scenario_correlated_exists,
    benchmark_scenario_indexed_update,
    benchmark_scenario_foreign_key_insert,
    benchmark_scenario_foreign_key_cascade,
};

struct benchmark_options {
    size_t row_count;
    size_t sample_count;
    size_t warmup_iterations;
    size_t iteration_override;
    const char *scenario_name;
    const char *database_directory;
    const char *output_path;
    bool keep_databases;
    bool list_scenarios;
    bool show_help;
};

struct benchmark_scenario {
    const char *name;
    enum benchmark_scenario_id id;
    enum benchmark_execution_mode mode;
    const char *sql;
    size_t default_iterations;
};

struct benchmark_database {
    enum benchmark_engine_kind kind;
    mylite_db *mylite;
    sqlite3 *sqlite;
    const char *path;
};

struct benchmark_statement {
    enum benchmark_engine_kind kind;
    mylite_stmt *mylite;
    sqlite3_stmt *sqlite;
};

struct benchmark_measurement {
    uint64_t elapsed_ns;
    uint64_t checksum;
    size_t operation_count;
    size_t result_row_count;
    size_t result_value_bytes;
    uint64_t affected_rows;
#ifdef MYLITE_ENABLE_PROFILING
    struct mylite_profile_snapshot profile;
#endif
};

struct benchmark_load_measurement {
    uint64_t total_ns;
    uint64_t accounts_ns;
    uint64_t items_ns;
    uint64_t tags_ns;
};

struct benchmark_dataset {
    size_t row_count;
    size_t account_count;
    size_t tag_count;
};

struct benchmark_paths {
    char mylite[path_capacity];
    char sqlite[path_capacity];
};

struct benchmark_sample_pair {
    struct benchmark_measurement mylite;
    struct benchmark_measurement sqlite;
};

static int parse_options(int argc, char **argv, struct benchmark_options *out_options);
static bool parse_flag_option(const char *argument, struct benchmark_options *out_options);
static bool option_takes_value(const char *argument);
static int parse_named_option(
    const char *argument,
    struct benchmark_options *out_options,
    const char *value
);
static void print_usage(const char *program_name, FILE *stream);
static void print_scenarios(FILE *stream);
static int run_benchmark(const struct benchmark_options *options);
static int make_database_paths(
    const struct benchmark_options *options,
    struct benchmark_paths *out_paths
);
static int open_benchmark_database(
    enum benchmark_engine_kind kind,
    const char *path,
    struct benchmark_database *out_database
);
static int create_benchmark_schema(struct benchmark_database *database);
static int seed_benchmark_database(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset,
    struct benchmark_load_measurement *out_measurement
);
static int verify_benchmark_database(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset
);
static void print_csv_header(FILE *output);
static void print_load_measurements(
    FILE *output,
    const struct benchmark_dataset *dataset,
    const struct benchmark_load_measurement *mylite,
    const struct benchmark_load_measurement *sqlite
);
static int run_scenarios(
    FILE *output,
    struct benchmark_database *mylite,
    struct benchmark_database *sqlite,
    const struct benchmark_dataset *dataset,
    const struct benchmark_options *options
);
static int run_scenario_samples(
    FILE *output,
    const struct benchmark_scenario *scenario,
    struct benchmark_database *mylite,
    struct benchmark_database *sqlite,
    const struct benchmark_dataset *dataset,
    const struct benchmark_options *options
);
static int run_scenario_engine(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t iterations,
    size_t warmup_iterations,
    struct benchmark_measurement *out_measurement
);
static int run_scenario_phase(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    struct benchmark_statement *prepared_statement,
    size_t iterations,
    struct benchmark_measurement *out_measurement
);
static int execute_scenario_iteration(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    struct benchmark_statement *prepared_statement,
    size_t iteration,
    struct benchmark_measurement *measurement
);
static int bind_scenario_parameters(
    struct benchmark_statement *statement,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t iteration
);
static int consume_statement(
    struct benchmark_database *database,
    struct benchmark_statement *statement,
    bool include_affected_rows,
    struct benchmark_measurement *measurement
);
static int consume_mylite_statement(
    struct benchmark_database *database,
    struct benchmark_statement *statement,
    bool include_affected_rows,
    struct benchmark_measurement *measurement
);
static int consume_sqlite_statement(
    struct benchmark_statement *statement,
    bool include_affected_rows,
    struct benchmark_measurement *measurement
);
static void print_sample(
    FILE *output,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    enum benchmark_engine_kind engine,
    size_t sample,
    size_t iterations,
    const struct benchmark_measurement *measurement
);
static void print_summary(
    FILE *output,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t iterations,
    const struct benchmark_sample_pair *samples,
    size_t sample_count
);
static int verify_sample_pair(
    const struct benchmark_scenario *scenario,
    size_t sample,
    const struct benchmark_sample_pair *pair
);
static int begin_transaction(struct benchmark_database *database);
static int commit_transaction(struct benchmark_database *database);
static int rollback_transaction(struct benchmark_database *database);
static int prepare_statement(
    struct benchmark_database *database,
    const char *sql,
    struct benchmark_statement *out_statement
);
static int reset_statement(struct benchmark_statement *statement);
static int finalize_statement(struct benchmark_statement *statement);
static int bind_int64(struct benchmark_statement *statement, size_t index, int64_t value);
static int bind_text(struct benchmark_statement *statement, size_t index, const char *value);
static int execute_sql(struct benchmark_database *database, const char *sql);
static int fetch_scalar_count(
    struct benchmark_database *database,
    const char *sql,
    uint64_t *out_value
);
static int seed_accounts(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset
);
static int seed_items(struct benchmark_database *database, const struct benchmark_dataset *dataset);
static int seed_tags(struct benchmark_database *database, const struct benchmark_dataset *dataset);
static int insert_account(struct benchmark_statement *statement, size_t account_id, size_t region);
static int insert_item(
    struct benchmark_statement *statement,
    size_t item_id,
    const struct benchmark_dataset *dataset
);
static int insert_tag(
    struct benchmark_statement *statement,
    size_t item_id,
    const struct benchmark_dataset *dataset
);
static int step_write_statement(struct benchmark_statement *statement);
static void close_benchmark_database(struct benchmark_database *database);
static void remove_database_files(const char *path);
static uint64_t file_size(const char *path);
static uint64_t monotonic_now_ns(void);
static uint64_t hash_bytes(uint64_t hash, const void *bytes, size_t size);
static void hash_uint64(uint64_t *hash, uint64_t value);
static double median_elapsed_us(
    enum benchmark_engine_kind engine,
    const struct benchmark_sample_pair *samples,
    size_t sample_count
);
static int compare_double(const void *left, const void *right);
static const char *engine_name(enum benchmark_engine_kind kind);
static const char *mode_name(enum benchmark_execution_mode mode);
static size_t account_count_for_rows(size_t row_count);
static size_t tag_count_for_rows(size_t row_count);
static size_t scenario_iterations(
    const struct benchmark_scenario *scenario,
    const struct benchmark_options *options
);
static int parse_size(const char *text, bool allow_zero, size_t *out_value);
static const char *option_value(int argc, char **argv, int *index);
static const char *default_database_directory(void);
static long benchmark_process_id(void);

static const struct benchmark_scenario benchmark_scenarios[] = {
    {
        "point_lookup_prepare_each",
        benchmark_scenario_point_lookup_prepare_each,
        benchmark_execution_prepare_each,
        "SELECT id, account_id, score FROM items WHERE id = ?",
        1000,
    },
    {
        "point_lookup_prepared",
        benchmark_scenario_point_lookup_prepared,
        benchmark_execution_prepared,
        "SELECT id, account_id, score FROM items WHERE id = ?",
        5000,
    },
    {
        "secondary_lookup",
        benchmark_scenario_secondary_lookup,
        benchmark_execution_prepared,
        "SELECT id, created_at FROM items WHERE account_id = ? "
        "ORDER BY created_at DESC, id DESC LIMIT 20",
        500,
    },
    {
        "range_aggregate",
        benchmark_scenario_range_aggregate,
        benchmark_execution_prepared,
        "SELECT COUNT(*), SUM(score) FROM items "
        "WHERE category_id = ? AND score BETWEEN ? AND ?",
        100,
    },
    {
        "full_scan_expression",
        benchmark_scenario_full_scan_expression,
        benchmark_execution_prepared,
        "SELECT COUNT(*), SUM(score) FROM items "
        "WHERE (score * 115 + category_id) > 10000000",
        3,
    },
    {
        "text_expression",
        benchmark_scenario_text_expression,
        benchmark_execution_prepared,
        "SELECT id, LENGTH(title) FROM items "
        "WHERE status = ? AND title LIKE 'item-00000%' ORDER BY id",
        3,
    },
    {
        "group_aggregate",
        benchmark_scenario_group_aggregate,
        benchmark_execution_prepared,
        "SELECT category_id, COUNT(*), SUM(score) FROM items "
        "GROUP BY category_id ORDER BY category_id",
        3,
    },
    {
        "indexed_order_limit",
        benchmark_scenario_indexed_order_limit,
        benchmark_execution_prepared,
        "SELECT id, created_at FROM items WHERE status = ? "
        "ORDER BY created_at DESC, id DESC LIMIT 100",
        100,
    },
    {
        "parent_join",
        benchmark_scenario_parent_join,
        benchmark_execution_prepared,
        "SELECT a.region, COUNT(*), SUM(i.score) FROM accounts AS a "
        "JOIN items AS i ON i.account_id = a.id "
        "WHERE a.region = ? GROUP BY a.region",
        20,
    },
    {
        "bridge_join",
        benchmark_scenario_bridge_join,
        benchmark_execution_prepared,
        "SELECT t.item_id, i.score + t.weight FROM item_tags AS t "
        "JOIN items AS i ON i.id = t.item_id WHERE t.tag_id BETWEEN ? AND ? "
        "ORDER BY t.tag_id, t.item_id",
        20,
    },
    {
        "correlated_exists",
        benchmark_scenario_correlated_exists,
        benchmark_execution_prepared,
        "SELECT a.id FROM accounts AS a WHERE EXISTS "
        "(SELECT 1 FROM items AS i WHERE i.account_id = a.id AND i.score > ?) ORDER BY a.id",
        10,
    },
    {
        "indexed_update_rollback",
        benchmark_scenario_indexed_update,
        benchmark_execution_write_rollback,
        "UPDATE items SET score = score + 1 WHERE id = ?",
        500,
    },
    {
        "foreign_key_insert_rollback",
        benchmark_scenario_foreign_key_insert,
        benchmark_execution_write_rollback,
        "INSERT INTO write_log (id, item_id, note) VALUES (?, ?, ?)",
        500,
    },
    {
        "foreign_key_cascade_rollback",
        benchmark_scenario_foreign_key_cascade,
        benchmark_execution_write_rollback,
        "DELETE FROM items WHERE id = ?",
        5,
    },
};

int main(int argc, char **argv) {
    struct benchmark_options options = {
        .row_count = default_row_count,
        .sample_count = default_sample_count,
        .warmup_iterations = default_warmup_iterations,
        .iteration_override = 0U,
        .scenario_name = NULL,
        .database_directory = NULL,
        .output_path = NULL,
        .keep_databases = false,
        .list_scenarios = false,
        .show_help = false,
    };

    if (parse_options(argc, argv, &options) != 0) {
        print_usage(argv[0], stderr);
        return 1;
    }
    if (options.show_help) {
        print_usage(argv[0], stdout);
        return 0;
    }
    if (options.list_scenarios) {
        print_scenarios(stdout);
        return 0;
    }
    return run_benchmark(&options);
}

static int parse_options(int argc, char **argv, struct benchmark_options *out_options) {
    for (int index = 1; index < argc; ++index) {
        const char *argument = argv[index];
        const char *value = NULL;

        if (parse_flag_option(argument, out_options)) {
            continue;
        }
        if (!option_takes_value(argument)) {
            fprintf(stderr, "unknown argument: %s\n", argument);
            return 1;
        }
        value = option_value(argc, argv, &index);
        if (value == NULL || parse_named_option(argument, out_options, value) != 0) {
            fprintf(stderr, "invalid or missing %s value\n", argument);
            return 1;
        }
    }
    return 0;
}

static bool parse_flag_option(const char *argument, struct benchmark_options *out_options) {
    if (strcmp(argument, "--help") == 0) {
        out_options->show_help = true;
        return true;
    }
    if (strcmp(argument, "--list") == 0) {
        out_options->list_scenarios = true;
        return true;
    }
    if (strcmp(argument, "--keep-databases") == 0) {
        out_options->keep_databases = true;
        return true;
    }
    return false;
}

static bool option_takes_value(const char *argument) {
    return strcmp(argument, "--rows") == 0 || strcmp(argument, "--samples") == 0 ||
           strcmp(argument, "--warmup") == 0 || strcmp(argument, "--iterations") == 0 ||
           strcmp(argument, "--scenario") == 0 || strcmp(argument, "--database-dir") == 0 ||
           strcmp(argument, "--output") == 0;
}

static int parse_named_option(
    const char *argument,
    struct benchmark_options *out_options,
    const char *value
) {
    if (strcmp(argument, "--rows") == 0) {
        return parse_size(value, false, &out_options->row_count);
    }
    if (strcmp(argument, "--samples") == 0) {
        return parse_size(value, false, &out_options->sample_count);
    }
    if (strcmp(argument, "--warmup") == 0) {
        return parse_size(value, true, &out_options->warmup_iterations);
    }
    if (strcmp(argument, "--iterations") == 0) {
        return parse_size(value, false, &out_options->iteration_override);
    }
    if (strcmp(argument, "--scenario") == 0) {
        out_options->scenario_name = value;
    } else if (strcmp(argument, "--database-dir") == 0) {
        out_options->database_directory = value;
    } else if (strcmp(argument, "--output") == 0) {
        out_options->output_path = value;
    } else {
        return 1;
    }
    return 0;
}

static void print_usage(const char *program_name, FILE *stream) {
    fprintf(
        stream,
        "usage: %s [--rows N] [--samples N] [--warmup N] [--iterations N]\n"
        "          [--scenario NAME] [--database-dir PATH] [--output PATH]\n"
        "          [--keep-databases] [--list] [--help]\n",
        program_name
    );
}

static void print_scenarios(FILE *stream) {
    for (size_t index = 0U; index < sizeof(benchmark_scenarios) / sizeof(benchmark_scenarios[0]);
         ++index) {
        fprintf(
            stream,
            "%s\t%s\t%zu\n",
            benchmark_scenarios[index].name,
            mode_name(benchmark_scenarios[index].mode),
            benchmark_scenarios[index].default_iterations
        );
    }
}

static int run_benchmark(const struct benchmark_options *options) {
    struct benchmark_paths paths = {{0}, {0}};
    struct benchmark_database mylite = {0};
    struct benchmark_database sqlite = {0};
    struct benchmark_load_measurement mylite_load = {0};
    struct benchmark_load_measurement sqlite_load = {0};
    const struct benchmark_dataset dataset = {
        .row_count = options->row_count,
        .account_count = account_count_for_rows(options->row_count),
        .tag_count = tag_count_for_rows(options->row_count),
    };
    FILE *output = stdout;
    int result = 1;

    if (options->output_path != NULL) {
        output = fopen(options->output_path, "wb");
        if (output == NULL) {
            fprintf(
                stderr,
                "failed to open output %s: %s\n",
                options->output_path,
                strerror(errno)
            );
            return 1;
        }
    }
    if (make_database_paths(options, &paths) != 0) {
        goto cleanup;
    }
    remove_database_files(paths.mylite);
    remove_database_files(paths.sqlite);
    fprintf(
        stderr,
        "large-dataset: rows=%zu accounts=%zu tags=%zu\n",
        dataset.row_count,
        dataset.account_count,
        dataset.tag_count
    );
    if (open_benchmark_database(benchmark_engine_mylite, paths.mylite, &mylite) != 0 ||
        open_benchmark_database(benchmark_engine_sqlite, paths.sqlite, &sqlite) != 0) {
        goto cleanup;
    }
    if (create_benchmark_schema(&mylite) != 0 || create_benchmark_schema(&sqlite) != 0) {
        goto cleanup;
    }
    fprintf(stderr, "large-dataset: seeding MyLite\n");
    if (seed_benchmark_database(&mylite, &dataset, &mylite_load) != 0) {
        goto cleanup;
    }
    fprintf(stderr, "large-dataset: seeding SQLite\n");
    if (seed_benchmark_database(&sqlite, &dataset, &sqlite_load) != 0) {
        goto cleanup;
    }
    if (verify_benchmark_database(&mylite, &dataset) != 0 ||
        verify_benchmark_database(&sqlite, &dataset) != 0) {
        goto cleanup;
    }
    print_csv_header(output);
    print_load_measurements(output, &dataset, &mylite_load, &sqlite_load);
    fprintf(
        output,
        "size,database_file,%zu,mylite,setup,0,1,0,%" PRIu64 ",0,0.000,0.000,0.000,0.000\n",
        dataset.row_count,
        file_size(paths.mylite)
    );
    fprintf(
        output,
        "size,database_file,%zu,sqlite,setup,0,1,0,%" PRIu64 ",0,0.000,0.000,0.000,0.000\n",
        dataset.row_count,
        file_size(paths.sqlite)
    );
    if (run_scenarios(output, &mylite, &sqlite, &dataset, options) != 0) {
        goto cleanup;
    }
    result = 0;

cleanup:
    close_benchmark_database(&sqlite);
    close_benchmark_database(&mylite);
    if (!options->keep_databases) {
        remove_database_files(paths.sqlite);
        remove_database_files(paths.mylite);
    } else if (paths.mylite[0] != '\0') {
        fprintf(stderr, "large-dataset: kept %s and %s\n", paths.mylite, paths.sqlite);
    }
    if (output != stdout) {
        if (fclose(output) != 0) {
            result = 1;
        }
    }
    return result;
}

static int make_database_paths(
    const struct benchmark_options *options,
    struct benchmark_paths *out_paths
) {
    const char *directory = options->database_directory == NULL ? default_database_directory()
                                                                : options->database_directory;
#if defined(_WIN32)
    const char separator = '\\';
#else
    const char separator = '/';
#endif
    int mylite_written = snprintf(
        out_paths->mylite,
        sizeof(out_paths->mylite),
        "%s%cmylite_large_%ld_%zu.mylite",
        directory,
        separator,
        benchmark_process_id(),
        options->row_count
    );
    int sqlite_written = snprintf(
        out_paths->sqlite,
        sizeof(out_paths->sqlite),
        "%s%cmylite_large_%ld_%zu.sqlite",
        directory,
        separator,
        benchmark_process_id(),
        options->row_count
    );

    if (mylite_written < 0 || (size_t)mylite_written >= sizeof(out_paths->mylite) ||
        sqlite_written < 0 || (size_t)sqlite_written >= sizeof(out_paths->sqlite)) {
        fprintf(stderr, "large-dataset: database path is too long\n");
        return 1;
    }
    return 0;
}

static int open_benchmark_database(
    enum benchmark_engine_kind kind,
    const char *path,
    struct benchmark_database *out_database
) {
    int rc = 0;

    *out_database = (struct benchmark_database){
        .kind = kind,
        .mylite = NULL,
        .sqlite = NULL,
        .path = path,
    };
    if (kind == benchmark_engine_mylite) {
        rc = mylite_open(path, &out_database->mylite);
        if (rc != MYLITE_OK) {
            fprintf(stderr, "large-dataset: MyLite open failed: %d\n", rc);
            return 1;
        }
        return 0;
    }
    rc = sqlite3_open(path, &out_database->sqlite);
    if (rc != SQLITE_OK) {
        fprintf(
            stderr,
            "large-dataset: SQLite open failed: %s\n",
            sqlite3_errmsg(out_database->sqlite)
        );
        return 1;
    }
    if (sqlite3_busy_timeout(out_database->sqlite, sqlite_busy_timeout_ms) != SQLITE_OK ||
        sqlite3_exec(
            out_database->sqlite,
            "PRAGMA journal_mode=DELETE; PRAGMA foreign_keys=ON; PRAGMA trusted_schema=OFF",
            NULL,
            NULL,
            NULL
        ) != SQLITE_OK) {
        fprintf(
            stderr,
            "large-dataset: SQLite configuration failed: %s\n",
            sqlite3_errmsg(out_database->sqlite)
        );
        return 1;
    }
    return 0;
}

static int create_benchmark_schema(struct benchmark_database *database) {
    static const char *const mylite_schema[] = {
        "CREATE DATABASE perf",
        "USE perf",
        "CREATE TABLE accounts ("
        "id BIGINT NOT NULL, region INT NOT NULL, name VARCHAR(64) NOT NULL,"
        "PRIMARY KEY (id), KEY idx_accounts_region (region, id))",
        "CREATE TABLE items ("
        "id BIGINT NOT NULL, account_id BIGINT NOT NULL, category_id INT NOT NULL,"
        "status VARCHAR(16) NOT NULL, score BIGINT NOT NULL, created_at BIGINT NOT NULL,"
        "title VARCHAR(64) NOT NULL, payload VARCHAR(160) NOT NULL,"
        "PRIMARY KEY (id),"
        "KEY idx_items_account_created (account_id, created_at, id),"
        "KEY idx_items_account_score (account_id, score),"
        "KEY idx_items_category_score (category_id, score),"
        "KEY idx_items_status_created (status, created_at, id),"
        "CONSTRAINT fk_items_account FOREIGN KEY (account_id) "
        "REFERENCES accounts (id) ON DELETE CASCADE)",
        "CREATE TABLE item_tags ("
        "item_id BIGINT NOT NULL, tag_id INT NOT NULL, weight INT NOT NULL,"
        "PRIMARY KEY (item_id, tag_id),"
        "KEY idx_item_tags_tag_weight (tag_id, weight, item_id),"
        "CONSTRAINT fk_item_tags_item FOREIGN KEY (item_id) "
        "REFERENCES items (id) ON DELETE CASCADE)",
        "CREATE TABLE write_log ("
        "id BIGINT NOT NULL, item_id BIGINT NOT NULL, note VARCHAR(64) NOT NULL,"
        "PRIMARY KEY (id), KEY idx_write_log_item (item_id),"
        "CONSTRAINT fk_write_log_item FOREIGN KEY (item_id) REFERENCES items (id))",
    };
    static const char *const sqlite_schema[] = {
        "CREATE TABLE accounts ("
        "id BIGINT NOT NULL PRIMARY KEY, region INTEGER NOT NULL, name TEXT NOT NULL)",
        "CREATE INDEX idx_accounts_region ON accounts(region, id)",
        "CREATE TABLE items ("
        "id BIGINT NOT NULL PRIMARY KEY, account_id BIGINT NOT NULL, category_id INTEGER NOT NULL,"
        "status TEXT NOT NULL, score BIGINT NOT NULL, created_at BIGINT NOT NULL,"
        "title TEXT NOT NULL, payload TEXT NOT NULL,"
        "FOREIGN KEY (account_id) REFERENCES accounts(id) ON DELETE CASCADE)",
        "CREATE INDEX idx_items_account_created ON items(account_id, created_at, id)",
        "CREATE INDEX idx_items_account_score ON items(account_id, score)",
        "CREATE INDEX idx_items_category_score ON items(category_id, score)",
        "CREATE INDEX idx_items_status_created ON items(status, created_at, id)",
        "CREATE TABLE item_tags ("
        "item_id BIGINT NOT NULL, tag_id INTEGER NOT NULL, weight INTEGER NOT NULL,"
        "PRIMARY KEY (item_id, tag_id),"
        "FOREIGN KEY (item_id) REFERENCES items(id) ON DELETE CASCADE)",
        "CREATE INDEX idx_item_tags_tag_weight ON item_tags(tag_id, weight, item_id)",
        "CREATE TABLE write_log ("
        "id BIGINT NOT NULL PRIMARY KEY, item_id BIGINT NOT NULL, note TEXT NOT NULL,"
        "FOREIGN KEY (item_id) REFERENCES items(id))",
        "CREATE INDEX idx_write_log_item ON write_log(item_id)",
    };
    const char *const *queries =
        database->kind == benchmark_engine_mylite ? mylite_schema : sqlite_schema;
    size_t query_count = database->kind == benchmark_engine_mylite
                             ? sizeof(mylite_schema) / sizeof(mylite_schema[0])
                             : sizeof(sqlite_schema) / sizeof(sqlite_schema[0]);

    for (size_t index = 0U; index < query_count; ++index) {
        if (execute_sql(database, queries[index]) != 0) {
            fprintf(
                stderr,
                "large-dataset: %s schema statement %zu failed\n",
                engine_name(database->kind),
                index + 1U
            );
            return 1;
        }
    }
    return 0;
}

static int seed_benchmark_database(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset,
    struct benchmark_load_measurement *out_measurement
) {
    uint64_t total_started = monotonic_now_ns();
    uint64_t phase_started = 0U;

    if (begin_transaction(database) != 0) {
        return 1;
    }
    phase_started = monotonic_now_ns();
    if (seed_accounts(database, dataset) != 0) {
        (void)rollback_transaction(database);
        return 1;
    }
    out_measurement->accounts_ns = monotonic_now_ns() - phase_started;
    phase_started = monotonic_now_ns();
    if (seed_items(database, dataset) != 0) {
        (void)rollback_transaction(database);
        return 1;
    }
    out_measurement->items_ns = monotonic_now_ns() - phase_started;
    phase_started = monotonic_now_ns();
    if (seed_tags(database, dataset) != 0) {
        (void)rollback_transaction(database);
        return 1;
    }
    out_measurement->tags_ns = monotonic_now_ns() - phase_started;
    if (commit_transaction(database) != 0) {
        return 1;
    }
    out_measurement->total_ns = monotonic_now_ns() - total_started;
    return 0;
}

static int verify_benchmark_database(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset
) {
    static const char *const queries[] = {
        "SELECT COUNT(*) FROM accounts",
        "SELECT COUNT(*) FROM items",
        "SELECT COUNT(*) FROM item_tags",
        "SELECT COUNT(*) FROM write_log",
    };
    const uint64_t expected[] = {
        dataset->account_count,
        dataset->row_count,
        dataset->row_count,
        0U,
    };

    for (size_t index = 0U; index < sizeof(queries) / sizeof(queries[0]); ++index) {
        uint64_t actual = 0U;

        if (fetch_scalar_count(database, queries[index], &actual) != 0 ||
            actual != expected[index]) {
            fprintf(
                stderr,
                "large-dataset: %s verification %zu expected %" PRIu64 ", got %" PRIu64 "\n",
                engine_name(database->kind),
                index,
                expected[index],
                actual
            );
            return 1;
        }
    }
    return 0;
}

static void print_csv_header(FILE *output) {
    fprintf(
        output,
        "record,scenario,rows,engine,mode,sample,iterations,result_rows,value_bytes,"
        "checksum,total_ms,avg_us,ops_per_sec,ratio\n"
    );
}

static void print_load_measurements(
    FILE *output,
    const struct benchmark_dataset *dataset,
    const struct benchmark_load_measurement *mylite,
    const struct benchmark_load_measurement *sqlite
) {
    static const char *const names[] = {"load.total", "load.accounts", "load.items", "load.tags"};
    const uint64_t mylite_values[] = {
        mylite->total_ns,
        mylite->accounts_ns,
        mylite->items_ns,
        mylite->tags_ns,
    };
    const uint64_t sqlite_values[] = {
        sqlite->total_ns,
        sqlite->accounts_ns,
        sqlite->items_ns,
        sqlite->tags_ns,
    };
    const size_t operations[] = {
        dataset->account_count + (dataset->row_count * 2U),
        dataset->account_count,
        dataset->row_count,
        dataset->row_count,
    };

    for (size_t index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        double mylite_ms = (double)mylite_values[index] / (double)nanoseconds_per_millisecond;
        double sqlite_ms = (double)sqlite_values[index] / (double)nanoseconds_per_millisecond;
        double mylite_us = (double)mylite_values[index] / (double)nanoseconds_per_microsecond /
                           (double)operations[index];
        double sqlite_us = (double)sqlite_values[index] / (double)nanoseconds_per_microsecond /
                           (double)operations[index];
        double ratio = sqlite_values[index] == 0U
                           ? 0.0
                           : (double)mylite_values[index] / (double)sqlite_values[index];

        fprintf(
            output,
            "summary,%s,%zu,mylite,load,0,%zu,0,0,0,%.3f,%.3f,%.3f,%.3f\n",
            names[index],
            dataset->row_count,
            operations[index],
            mylite_ms,
            mylite_us,
            mylite_values[index] == 0U
                ? 0.0
                : (double)operations[index] * (double)nanoseconds_per_second /
                      (double)mylite_values[index],
            ratio
        );
        fprintf(
            output,
            "summary,%s,%zu,sqlite,load,0,%zu,0,0,0,%.3f,%.3f,%.3f,1.000\n",
            names[index],
            dataset->row_count,
            operations[index],
            sqlite_ms,
            sqlite_us,
            sqlite_values[index] == 0U
                ? 0.0
                : (double)operations[index] * (double)nanoseconds_per_second /
                      (double)sqlite_values[index]
        );
    }
}

static int run_scenarios(
    FILE *output,
    struct benchmark_database *mylite,
    struct benchmark_database *sqlite,
    const struct benchmark_dataset *dataset,
    const struct benchmark_options *options
) {
    bool matched = options->scenario_name == NULL;

    for (size_t index = 0U; index < sizeof(benchmark_scenarios) / sizeof(benchmark_scenarios[0]);
         ++index) {
        const struct benchmark_scenario *scenario = &benchmark_scenarios[index];

        if (options->scenario_name != NULL && strcmp(options->scenario_name, scenario->name) != 0) {
            continue;
        }
        matched = true;
        fprintf(
            stderr,
            "large-dataset: scenario=%s iterations=%zu samples=%zu\n",
            scenario->name,
            scenario_iterations(scenario, options),
            options->sample_count
        );
        if (run_scenario_samples(output, scenario, mylite, sqlite, dataset, options) != 0) {
            return 1;
        }
        fflush(output);
    }
    if (!matched) {
        fprintf(stderr, "large-dataset: unknown scenario %s\n", options->scenario_name);
        return 1;
    }
    return 0;
}

static int run_scenario_samples(
    FILE *output,
    const struct benchmark_scenario *scenario,
    struct benchmark_database *mylite,
    struct benchmark_database *sqlite,
    const struct benchmark_dataset *dataset,
    const struct benchmark_options *options
) {
    struct benchmark_sample_pair *samples = calloc(options->sample_count, sizeof(*samples));
    size_t iterations = scenario_iterations(scenario, options);
    int result = 1;

    if (samples == NULL) {
        fprintf(stderr, "large-dataset: failed to allocate samples\n");
        return 1;
    }
    for (size_t sample = 0U; sample < options->sample_count; ++sample) {
        struct benchmark_database *first = sample % 2U == 0U ? mylite : sqlite;
        struct benchmark_database *second = sample % 2U == 0U ? sqlite : mylite;
        struct benchmark_measurement *first_measurement = first->kind == benchmark_engine_mylite
                                                              ? &samples[sample].mylite
                                                              : &samples[sample].sqlite;
        struct benchmark_measurement *second_measurement = second->kind == benchmark_engine_mylite
                                                               ? &samples[sample].mylite
                                                               : &samples[sample].sqlite;

        if (run_scenario_engine(
                first,
                scenario,
                dataset,
                iterations,
                options->warmup_iterations,
                first_measurement
            ) != 0 ||
            run_scenario_engine(
                second,
                scenario,
                dataset,
                iterations,
                options->warmup_iterations,
                second_measurement
            ) != 0 ||
            verify_sample_pair(scenario, sample, &samples[sample]) != 0) {
            goto cleanup;
        }
        print_sample(
            output,
            scenario,
            dataset,
            benchmark_engine_mylite,
            sample + 1U,
            iterations,
            &samples[sample].mylite
        );
        print_sample(
            output,
            scenario,
            dataset,
            benchmark_engine_sqlite,
            sample + 1U,
            iterations,
            &samples[sample].sqlite
        );
    }
    print_summary(output, scenario, dataset, iterations, samples, options->sample_count);
    result = 0;

cleanup:
    free(samples);
    return result;
}

static int run_scenario_engine(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t iterations,
    size_t warmup_iterations,
    struct benchmark_measurement *out_measurement
) {
    struct benchmark_statement statement = {0};
    struct benchmark_measurement warmup = {0};
    struct benchmark_statement *prepared_statement = NULL;
    uint64_t started = 0U;
#ifdef MYLITE_ENABLE_PROFILING
    bool profile_started = false;
#endif
    int result = 1;

    if (scenario->mode != benchmark_execution_prepare_each) {
        if (prepare_statement(database, scenario->sql, &statement) != 0) {
            return 1;
        }
        prepared_statement = &statement;
    }
    if (scenario->mode == benchmark_execution_write_rollback && begin_transaction(database) != 0) {
        goto cleanup;
    }
    if (warmup_iterations > 0U && run_scenario_phase(
                                      database,
                                      scenario,
                                      dataset,
                                      prepared_statement,
                                      warmup_iterations,
                                      &warmup
                                  ) != 0) {
        if (scenario->mode == benchmark_execution_write_rollback) {
            (void)rollback_transaction(database);
        }
        goto cleanup;
    }
    if (scenario->mode == benchmark_execution_write_rollback) {
        if (rollback_transaction(database) != 0 || begin_transaction(database) != 0) {
            goto cleanup;
        }
    }
    *out_measurement = (struct benchmark_measurement){0};
#ifdef MYLITE_ENABLE_PROFILING
    if (database->kind == benchmark_engine_mylite) {
        if (mylite_profile_start(database->mylite) != MYLITE_OK) {
            fprintf(stderr, "large-dataset: failed to start profile for %s\n", scenario->name);
            goto cleanup;
        }
        profile_started = true;
    }
#endif
    started = monotonic_now_ns();
    if (run_scenario_phase(
            database,
            scenario,
            dataset,
            prepared_statement,
            iterations,
            out_measurement
        ) != 0) {
        if (scenario->mode == benchmark_execution_write_rollback) {
            (void)rollback_transaction(database);
        }
#ifdef MYLITE_ENABLE_PROFILING
        if (profile_started) {
            (void)mylite_profile_stop(database->mylite, &out_measurement->profile);
            profile_started = false;
        }
#endif
        goto cleanup;
    }
    out_measurement->elapsed_ns = monotonic_now_ns() - started;
#ifdef MYLITE_ENABLE_PROFILING
    if (profile_started) {
        if (mylite_profile_stop(database->mylite, &out_measurement->profile) != MYLITE_OK) {
            fprintf(stderr, "large-dataset: failed to stop profile for %s\n", scenario->name);
            profile_started = false;
            goto cleanup;
        }
        profile_started = false;
        fprintf(
            stderr,
            "large-dataset-profile: scenario=%s iterations=%zu total_ms=%.3f "
            "api_ms=%.3f sqlite_step_ms=%.3f metadata_step_ms=%.3f cursor_step_ms=%.3f "
            "cursor_finalize_ms=%.3f statements=%" PRIu64 " sqlite_steps=%" PRIu64
            " metadata_steps=%" PRIu64 "\n",
            scenario->name,
            iterations,
            (double)out_measurement->elapsed_ns / (double)nanoseconds_per_millisecond,
            (double)out_measurement->profile.statement_api_ns / (double)nanoseconds_per_millisecond,
            (double)out_measurement->profile.sqlite_step_ns / (double)nanoseconds_per_millisecond,
            (double)out_measurement->profile.metadata_step_ns / (double)nanoseconds_per_millisecond,
            (double)out_measurement->profile.cursor_step_ns / (double)nanoseconds_per_millisecond,
            (double)out_measurement->profile.cursor_finalize_ns /
                (double)nanoseconds_per_millisecond,
            out_measurement->profile.statement_count,
            out_measurement->profile.sqlite_step_count,
            out_measurement->profile.metadata_step_count
        );
    }
#endif
    if (scenario->mode == benchmark_execution_write_rollback &&
        rollback_transaction(database) != 0) {
        goto cleanup;
    }
    result = 0;

cleanup:
#ifdef MYLITE_ENABLE_PROFILING
    if (profile_started) {
        (void)mylite_profile_stop(database->mylite, &out_measurement->profile);
    }
#endif
    if (prepared_statement != NULL && finalize_statement(prepared_statement) != 0) {
        result = 1;
    }
    return result;
}

static int run_scenario_phase(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    struct benchmark_statement *prepared_statement,
    size_t iterations,
    struct benchmark_measurement *out_measurement
) {
    out_measurement->checksum = fnv_offset_basis;

    for (size_t iteration = 0U; iteration < iterations; ++iteration) {
        if (execute_scenario_iteration(
                database,
                scenario,
                dataset,
                prepared_statement,
                iteration,
                out_measurement
            ) != 0) {
            return 1;
        }
        ++out_measurement->operation_count;
    }
    return 0;
}

static int execute_scenario_iteration(
    struct benchmark_database *database,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    struct benchmark_statement *prepared_statement,
    size_t iteration,
    struct benchmark_measurement *measurement
) {
    struct benchmark_statement local_statement = {0};
    struct benchmark_statement *statement = prepared_statement;
    bool local = false;
    int result = 1;

    if (statement == NULL) {
        if (prepare_statement(database, scenario->sql, &local_statement) != 0) {
            return 1;
        }
        statement = &local_statement;
        local = true;
    }
    if (reset_statement(statement) != 0) {
        fprintf(
            stderr,
            "large-dataset: %s reset failed for %s\n",
            engine_name(database->kind),
            scenario->name
        );
        goto cleanup;
    }
    if (bind_scenario_parameters(statement, scenario, dataset, iteration) != 0) {
        fprintf(
            stderr,
            "large-dataset: %s bind failed for %s\n",
            engine_name(database->kind),
            scenario->name
        );
        goto cleanup;
    }
    if (consume_statement(
            database,
            statement,
            scenario->mode == benchmark_execution_write_rollback,
            measurement
        ) != 0) {
        goto cleanup;
    }
    result = 0;

cleanup:
    if (local && finalize_statement(statement) != 0) {
        result = 1;
    }
    return result;
}

static int bind_scenario_parameters(
    struct benchmark_statement *statement,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t iteration
) {
    size_t pseudo_random = (iteration * score_multiplier) + pseudo_random_increment;
    int rc = 0;

    switch (scenario->id) {
    case benchmark_scenario_point_lookup_prepare_each:
    case benchmark_scenario_point_lookup_prepared:
    case benchmark_scenario_indexed_update:
        return bind_int64(statement, 0U, (int64_t)((pseudo_random % dataset->row_count) + 1U));
    case benchmark_scenario_secondary_lookup:
        return bind_int64(statement, 0U, (int64_t)((pseudo_random % dataset->account_count) + 1U));
    case benchmark_scenario_range_aggregate:
        rc = bind_int64(statement, 0U, (int64_t)(iteration % category_count));
        if (rc == 0) {
            rc = bind_int64(statement, 1U, score_range_lower);
        }
        if (rc == 0) {
            rc = bind_int64(statement, 2U, score_range_upper);
        }
        return rc;
    case benchmark_scenario_full_scan_expression:
        return 0;
    case benchmark_scenario_text_expression:
    case benchmark_scenario_indexed_order_limit:
        return bind_text(statement, 0U, "published");
    case benchmark_scenario_group_aggregate:
        return 0;
    case benchmark_scenario_parent_join:
        return bind_int64(statement, 0U, (int64_t)(iteration % account_region_count));
    case benchmark_scenario_bridge_join: {
        size_t lower = iteration % dataset->tag_count;
        size_t upper = lower + bridge_tag_span;

        if (upper >= dataset->tag_count) {
            upper = dataset->tag_count - 1U;
        }
        rc = bind_int64(statement, 0U, (int64_t)lower);
        if (rc == 0) {
            rc = bind_int64(statement, 1U, (int64_t)upper);
        }
        return rc;
    }
    case benchmark_scenario_correlated_exists:
        return bind_int64(statement, 0U, correlated_score_threshold);
    case benchmark_scenario_foreign_key_insert: {
        char note[generated_text_capacity];
        int written = snprintf(note, sizeof(note), "write-%010zu", iteration);

        if (written < 0 || (size_t)written >= sizeof(note)) {
            return 1;
        }
        rc = bind_int64(
            statement,
            0U,
            (int64_t)((dataset->row_count * write_log_id_factor) + iteration + 1U)
        );
        if (rc == 0) {
            rc = bind_int64(statement, 1U, (int64_t)((pseudo_random % dataset->row_count) + 1U));
        }
        if (rc == 0) {
            rc = bind_text(statement, 2U, note);
        }
        return rc;
    }
    case benchmark_scenario_foreign_key_cascade:
        return bind_int64(statement, 0U, (int64_t)((pseudo_random % dataset->row_count) + 1U));
    }
    return 1;
}

static int consume_statement(
    struct benchmark_database *database,
    struct benchmark_statement *statement,
    bool include_affected_rows,
    struct benchmark_measurement *measurement
) {
    if (statement->kind == benchmark_engine_mylite) {
        return consume_mylite_statement(database, statement, include_affected_rows, measurement);
    }
    return consume_sqlite_statement(statement, include_affected_rows, measurement);
}

static int consume_mylite_statement(
    struct benchmark_database *database,
    struct benchmark_statement *statement,
    bool include_affected_rows,
    struct benchmark_measurement *measurement
) {
    for (;;) {
        int rc = mylite_stmt_step(statement->mylite);

        if (rc == MYLITE_ROW) {
            size_t columns = mylite_stmt_column_count(statement->mylite);

            ++measurement->result_row_count;
            hash_uint64(&measurement->checksum, columns);
            for (size_t column = 0U; column < columns; ++column) {
                if (mylite_stmt_value_is_null(statement->mylite, column) != 0) {
                    hash_uint64(&measurement->checksum, UINT64_MAX);
                } else {
                    const void *bytes = mylite_stmt_value_bytes(statement->mylite, column);
                    size_t size = mylite_stmt_value_size(statement->mylite, column);

                    hash_uint64(&measurement->checksum, size);
                    measurement->checksum = hash_bytes(measurement->checksum, bytes, size);
                    measurement->result_value_bytes += size;
                }
            }
            continue;
        }
        if (rc != MYLITE_DONE) {
            fprintf(
                stderr,
                "large-dataset: MyLite step failed: %d %s\n",
                rc,
                mylite_errmsg(database->mylite)
            );
            return 1;
        }
        if (include_affected_rows) {
            uint64_t affected_rows = (uint64_t)mylite_stmt_affected_rows(statement->mylite);

            measurement->affected_rows += affected_rows;
            hash_uint64(&measurement->checksum, affected_rows);
        }
        return 0;
    }
}

static int consume_sqlite_statement(
    struct benchmark_statement *statement,
    bool include_affected_rows,
    struct benchmark_measurement *measurement
) {
    for (;;) {
        int rc = sqlite3_step(statement->sqlite);

        if (rc == SQLITE_ROW) {
            int columns = sqlite3_column_count(statement->sqlite);

            ++measurement->result_row_count;
            hash_uint64(&measurement->checksum, (uint64_t)columns);
            for (int column = 0; column < columns; ++column) {
                if (sqlite3_column_type(statement->sqlite, column) == SQLITE_NULL) {
                    hash_uint64(&measurement->checksum, UINT64_MAX);
                } else {
                    const void *bytes = sqlite3_column_blob(statement->sqlite, column);
                    int byte_count = sqlite3_column_bytes(statement->sqlite, column);
                    size_t size = byte_count < 0 ? 0U : (size_t)byte_count;

                    hash_uint64(&measurement->checksum, size);
                    measurement->checksum = hash_bytes(measurement->checksum, bytes, size);
                    measurement->result_value_bytes += size;
                }
            }
            continue;
        }
        if (rc != SQLITE_DONE) {
            fprintf(
                stderr,
                "large-dataset: SQLite step failed: %s\n",
                sqlite3_errmsg(sqlite3_db_handle(statement->sqlite))
            );
            return 1;
        }
        if (include_affected_rows) {
            uint64_t affected_rows =
                (uint64_t)sqlite3_changes64(sqlite3_db_handle(statement->sqlite));

            measurement->affected_rows += affected_rows;
            hash_uint64(&measurement->checksum, affected_rows);
        }
        return 0;
    }
}

static void print_sample(
    FILE *output,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    enum benchmark_engine_kind engine,
    size_t sample,
    size_t iterations,
    const struct benchmark_measurement *measurement
) {
    double total_ms = (double)measurement->elapsed_ns / (double)nanoseconds_per_millisecond;
    double average_us =
        (double)measurement->elapsed_ns / (double)nanoseconds_per_microsecond / (double)iterations;
    double operations_per_second =
        measurement->elapsed_ns == 0U
            ? 0.0
            : (double)iterations * (double)nanoseconds_per_second / (double)measurement->elapsed_ns;

    fprintf(
        output,
        "sample,%s,%zu,%s,%s,%zu,%zu,%zu,%zu,%" PRIu64 ",%.3f,%.3f,%.3f,0.000\n",
        scenario->name,
        dataset->row_count,
        engine_name(engine),
        mode_name(scenario->mode),
        sample,
        iterations,
        measurement->result_row_count,
        measurement->result_value_bytes,
        measurement->checksum,
        total_ms,
        average_us,
        operations_per_second
    );
}

static void print_summary(
    FILE *output,
    const struct benchmark_scenario *scenario,
    const struct benchmark_dataset *dataset,
    size_t iterations,
    const struct benchmark_sample_pair *samples,
    size_t sample_count
) {
    double mylite_us = median_elapsed_us(benchmark_engine_mylite, samples, sample_count);
    double sqlite_us = median_elapsed_us(benchmark_engine_sqlite, samples, sample_count);
    double ratio = sqlite_us == 0.0 ? 0.0 : mylite_us / sqlite_us;

    fprintf(
        output,
        "summary,%s,%zu,mylite,%s,0,%zu,0,0,0,%.3f,%.3f,%.3f,%.3f\n",
        scenario->name,
        dataset->row_count,
        mode_name(scenario->mode),
        iterations,
        mylite_us * (double)iterations / (double)milliseconds_per_second,
        mylite_us,
        mylite_us == 0.0 ? 0.0 : (double)microseconds_per_second / mylite_us,
        ratio
    );
    fprintf(
        output,
        "summary,%s,%zu,sqlite,%s,0,%zu,0,0,0,%.3f,%.3f,%.3f,1.000\n",
        scenario->name,
        dataset->row_count,
        mode_name(scenario->mode),
        iterations,
        sqlite_us * (double)iterations / (double)milliseconds_per_second,
        sqlite_us,
        sqlite_us == 0.0 ? 0.0 : (double)microseconds_per_second / sqlite_us
    );
}

static int verify_sample_pair(
    const struct benchmark_scenario *scenario,
    size_t sample,
    const struct benchmark_sample_pair *pair
) {
    if (pair->mylite.operation_count != pair->sqlite.operation_count ||
        pair->mylite.result_row_count != pair->sqlite.result_row_count ||
        pair->mylite.result_value_bytes != pair->sqlite.result_value_bytes ||
        pair->mylite.affected_rows != pair->sqlite.affected_rows ||
        pair->mylite.checksum != pair->sqlite.checksum) {
        fprintf(
            stderr,
            "large-dataset: %s sample %zu result mismatch\n"
            "  MyLite operations=%zu rows=%zu bytes=%zu affected=%" PRIu64 " checksum=%" PRIu64 "\n"
            "  SQLite operations=%zu rows=%zu bytes=%zu affected=%" PRIu64 " checksum=%" PRIu64
            "\n",
            scenario->name,
            sample + 1U,
            pair->mylite.operation_count,
            pair->mylite.result_row_count,
            pair->mylite.result_value_bytes,
            pair->mylite.affected_rows,
            pair->mylite.checksum,
            pair->sqlite.operation_count,
            pair->sqlite.result_row_count,
            pair->sqlite.result_value_bytes,
            pair->sqlite.affected_rows,
            pair->sqlite.checksum
        );
        return 1;
    }
    return 0;
}

static int begin_transaction(struct benchmark_database *database) {
    if (database->kind == benchmark_engine_mylite) {
        mylite_result *result = NULL;
        int rc = mylite_execute_transaction_control(
            database->mylite,
            MYLITE_TRANSACTION_CONTROL_START,
            &result
        );

        mylite_result_free(result);
        if (rc != MYLITE_OK) {
            fprintf(stderr, "large-dataset: MyLite transaction start failed\n");
            return 1;
        }
        return 0;
    }
    return execute_sql(database, "BEGIN");
}

static int commit_transaction(struct benchmark_database *database) {
    if (database->kind == benchmark_engine_mylite) {
        mylite_result *result = NULL;
        int rc = mylite_execute_transaction_control(
            database->mylite,
            MYLITE_TRANSACTION_CONTROL_COMMIT,
            &result
        );

        mylite_result_free(result);
        if (rc != MYLITE_OK) {
            fprintf(stderr, "large-dataset: MyLite transaction commit failed\n");
            return 1;
        }
        return 0;
    }
    return execute_sql(database, "COMMIT");
}

static int rollback_transaction(struct benchmark_database *database) {
    if (database->kind == benchmark_engine_mylite) {
        mylite_result *result = NULL;
        int rc = mylite_execute_transaction_control(
            database->mylite,
            MYLITE_TRANSACTION_CONTROL_ROLLBACK,
            &result
        );

        mylite_result_free(result);
        if (rc != MYLITE_OK) {
            fprintf(stderr, "large-dataset: MyLite transaction rollback failed\n");
            return 1;
        }
        return 0;
    }
    return execute_sql(database, "ROLLBACK");
}

static int prepare_statement(
    struct benchmark_database *database,
    const char *sql,
    struct benchmark_statement *out_statement
) {
    *out_statement = (struct benchmark_statement){
        .kind = database->kind,
        .mylite = NULL,
        .sqlite = NULL,
    };
    if (database->kind == benchmark_engine_mylite) {
        int rc = mylite_prepare(database->mylite, sql, strlen(sql), &out_statement->mylite);

        if (rc != MYLITE_OK) {
            fprintf(
                stderr,
                "large-dataset: MyLite prepare failed: %s\nSQL: %s\n",
                mylite_errmsg(database->mylite),
                sql
            );
            return 1;
        }
        return 0;
    }
    if (sqlite3_prepare_v2(database->sqlite, sql, -1, &out_statement->sqlite, NULL) != SQLITE_OK) {
        fprintf(
            stderr,
            "large-dataset: SQLite prepare failed: %s\nSQL: %s\n",
            sqlite3_errmsg(database->sqlite),
            sql
        );
        return 1;
    }
    return 0;
}

static int reset_statement(struct benchmark_statement *statement) {
    if (statement->kind == benchmark_engine_mylite) {
        return mylite_stmt_reset(statement->mylite) == MYLITE_OK ? 0 : 1;
    }
    if (sqlite3_reset(statement->sqlite) != SQLITE_OK ||
        sqlite3_clear_bindings(statement->sqlite) != SQLITE_OK) {
        return 1;
    }
    return 0;
}

static int finalize_statement(struct benchmark_statement *statement) {
    int rc = 0;

    if (statement->kind == benchmark_engine_mylite) {
        rc = mylite_stmt_finalize(statement->mylite);
        statement->mylite = NULL;
        if (rc != MYLITE_OK) {
            fprintf(stderr, "large-dataset: MyLite finalize failed: %d\n", rc);
        }
        return rc == MYLITE_OK ? 0 : 1;
    }
    rc = sqlite3_finalize(statement->sqlite);
    statement->sqlite = NULL;
    if (rc != SQLITE_OK) {
        fprintf(stderr, "large-dataset: SQLite finalize failed: %d\n", rc);
    }
    return rc == SQLITE_OK ? 0 : 1;
}

static int bind_int64(struct benchmark_statement *statement, size_t index, int64_t value) {
    if (statement->kind == benchmark_engine_mylite) {
        return mylite_stmt_bind_int64(statement->mylite, index, value) == MYLITE_OK ? 0 : 1;
    }
    return sqlite3_bind_int64(statement->sqlite, (int)index + 1, value) == SQLITE_OK ? 0 : 1;
}

static int bind_text(struct benchmark_statement *statement, size_t index, const char *value) {
    if (statement->kind == benchmark_engine_mylite) {
        return mylite_stmt_bind_text(statement->mylite, index, value, strlen(value)) == MYLITE_OK
                   ? 0
                   : 1;
    }
    return sqlite3_bind_text(statement->sqlite, (int)index + 1, value, -1, SQLITE_TRANSIENT) ==
                   SQLITE_OK
               ? 0
               : 1;
}

static int execute_sql(struct benchmark_database *database, const char *sql) {
    if (database->kind == benchmark_engine_mylite) {
        mylite_result *result = NULL;
        int rc = mylite_execute(database->mylite, sql, strlen(sql), &result);

        mylite_result_free(result);
        if (rc != MYLITE_OK) {
            fprintf(
                stderr,
                "large-dataset: MyLite SQL failed: %s\nSQL: %s\n",
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
            "large-dataset: SQLite SQL failed: %s\nSQL: %s\n",
            sqlite3_errmsg(database->sqlite),
            sql
        );
        return 1;
    }
    return 0;
}

static int fetch_scalar_count(
    struct benchmark_database *database,
    const char *sql,
    uint64_t *out_value
) {
    struct benchmark_statement statement = {0};
    int result = 1;

    *out_value = 0U;
    if (prepare_statement(database, sql, &statement) != 0 || reset_statement(&statement) != 0) {
        return 1;
    }
    if (database->kind == benchmark_engine_mylite) {
        if (mylite_stmt_step(statement.mylite) != MYLITE_ROW) {
            goto cleanup;
        }
        *out_value = strtoull(mylite_stmt_value_text(statement.mylite, 0U), NULL, decimal_base);
        if (mylite_stmt_step(statement.mylite) != MYLITE_DONE) {
            goto cleanup;
        }
    } else {
        if (sqlite3_step(statement.sqlite) != SQLITE_ROW) {
            goto cleanup;
        }
        *out_value = (uint64_t)sqlite3_column_int64(statement.sqlite, 0);
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

static int seed_accounts(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset
) {
    struct benchmark_statement statement = {0};
    int result = 1;

    if (prepare_statement(
            database,
            "INSERT INTO accounts (id, region, name) VALUES (?, ?, ?)",
            &statement
        ) != 0) {
        return 1;
    }
    for (size_t account_id = 1U; account_id <= dataset->account_count; ++account_id) {
        if (insert_account(&statement, account_id, account_id % account_region_count) != 0) {
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

static int seed_items(
    struct benchmark_database *database,
    const struct benchmark_dataset *dataset
) {
    struct benchmark_statement statement = {0};
    int result = 1;

    if (prepare_statement(
            database,
            "INSERT INTO items "
            "(id, account_id, category_id, status, score, created_at, title, payload) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
            &statement
        ) != 0) {
        return 1;
    }
    for (size_t item_id = 1U; item_id <= dataset->row_count; ++item_id) {
        if (insert_item(&statement, item_id, dataset) != 0) {
            goto cleanup;
        }
        if (item_id % seed_progress_interval == 0U) {
            fprintf(
                stderr,
                "large-dataset: %s seeded %zu/%zu items\n",
                engine_name(database->kind),
                item_id,
                dataset->row_count
            );
        }
    }
    result = 0;

cleanup:
    if (finalize_statement(&statement) != 0) {
        result = 1;
    }
    return result;
}

static int seed_tags(struct benchmark_database *database, const struct benchmark_dataset *dataset) {
    struct benchmark_statement statement = {0};
    int result = 1;

    if (prepare_statement(
            database,
            "INSERT INTO item_tags (item_id, tag_id, weight) VALUES (?, ?, ?)",
            &statement
        ) != 0) {
        return 1;
    }
    for (size_t item_id = 1U; item_id <= dataset->row_count; ++item_id) {
        if (insert_tag(&statement, item_id, dataset) != 0) {
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

static int insert_account(struct benchmark_statement *statement, size_t account_id, size_t region) {
    char name[generated_text_capacity];
    int written = snprintf(name, sizeof(name), "account-%010zu", account_id);

    if (written < 0 || (size_t)written >= sizeof(name) || reset_statement(statement) != 0 ||
        bind_int64(statement, 0U, (int64_t)account_id) != 0 ||
        bind_int64(statement, 1U, (int64_t)region) != 0 || bind_text(statement, 2U, name) != 0) {
        return 1;
    }
    return step_write_statement(statement);
}

static int insert_item(
    struct benchmark_statement *statement,
    size_t item_id,
    const struct benchmark_dataset *dataset
) {
    static const char *const statuses[] = {
        "published",
        "published",
        "published",
        "published",
        "published",
        "published",
        "published",
        "draft",
        "draft",
        "archived",
    };
    char title[generated_text_capacity];
    char payload[generated_text_capacity];
    size_t account_id = ((item_id - 1U) % dataset->account_count) + 1U;
    size_t category_id = item_id % category_count;
    int64_t score = (int64_t)((item_id * score_multiplier) % score_modulus);
    int title_written = snprintf(title, sizeof(title), "item-%010zu", item_id);
    int payload_written = snprintf(
        payload,
        sizeof(payload),
        "payload-%010zu-abcdefghijklmnopqrstuvwxyz-0123456789",
        item_id
    );

    if (title_written < 0 || (size_t)title_written >= sizeof(title) || payload_written < 0 ||
        (size_t)payload_written >= sizeof(payload) || reset_statement(statement) != 0 ||
        bind_int64(statement, 0U, (int64_t)item_id) != 0 ||
        bind_int64(statement, 1U, (int64_t)account_id) != 0 ||
        bind_int64(statement, 2U, (int64_t)category_id) != 0 ||
        bind_text(statement, 3U, statuses[item_id % (sizeof(statuses) / sizeof(statuses[0]))]) !=
            0 ||
        bind_int64(statement, 4U, score) != 0 ||
        bind_int64(
            statement,
            item_created_at_parameter,
            (int64_t)(seed_timestamp_base + (item_id % seconds_per_year))
        ) != 0 ||
        bind_text(statement, item_title_parameter, title) != 0 ||
        bind_text(statement, item_payload_parameter, payload) != 0) {
        return 1;
    }
    return step_write_statement(statement);
}

static int insert_tag(
    struct benchmark_statement *statement,
    size_t item_id,
    const struct benchmark_dataset *dataset
) {
    if (reset_statement(statement) != 0 || bind_int64(statement, 0U, (int64_t)item_id) != 0 ||
        bind_int64(statement, 1U, (int64_t)(item_id % dataset->tag_count)) != 0 ||
        bind_int64(statement, 2U, (int64_t)(item_id % tag_weight_modulus)) != 0) {
        return 1;
    }
    return step_write_statement(statement);
}

static int step_write_statement(struct benchmark_statement *statement) {
    if (statement->kind == benchmark_engine_mylite) {
        return mylite_stmt_step(statement->mylite) == MYLITE_DONE ? 0 : 1;
    }
    return sqlite3_step(statement->sqlite) == SQLITE_DONE ? 0 : 1;
}

static void close_benchmark_database(struct benchmark_database *database) {
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

static uint64_t file_size(const char *path) {
#if defined(_WIN32)
    struct _stat64 status = {0};

    return _stat64(path, &status) == 0 ? (uint64_t)status.st_size : 0U;
#else
    struct stat status = {0};

    return stat(path, &status) == 0 ? (uint64_t)status.st_size : 0U;
#endif
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

static uint64_t hash_bytes(uint64_t hash, const void *bytes, size_t size) {
    static const uint64_t fnv_prime = 1099511628211ULL;
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
        bytes[index] = (unsigned char)((value >> (index * bits_per_byte)) & byte_mask);
    }
    *hash = hash_bytes(*hash, bytes, sizeof(bytes));
}

static double median_elapsed_us(
    enum benchmark_engine_kind engine,
    const struct benchmark_sample_pair *samples,
    size_t sample_count
) {
    double *values = malloc(sample_count * sizeof(*values));
    double median = 0.0;

    if (values == NULL) {
        return 0.0;
    }
    for (size_t index = 0U; index < sample_count; ++index) {
        uint64_t elapsed = engine == benchmark_engine_mylite ? samples[index].mylite.elapsed_ns
                                                             : samples[index].sqlite.elapsed_ns;
        size_t operations = engine == benchmark_engine_mylite
                                ? samples[index].mylite.operation_count
                                : samples[index].sqlite.operation_count;

        values[index] = (double)elapsed / (double)nanoseconds_per_microsecond / (double)operations;
    }
    qsort(values, sample_count, sizeof(*values), compare_double);
    if (sample_count % 2U == 0U) {
        median = (values[(sample_count / 2U) - 1U] + values[sample_count / 2U]) /
                 (double)median_pair_count;
    } else {
        median = values[sample_count / 2U];
    }
    free(values);
    return median;
}

// qsort fixes the callback signature.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static int compare_double(const void *left, const void *right) {
    double left_value = *(const double *)left;
    double right_value = *(const double *)right;

    if (left_value < right_value) {
        return -1;
    }
    if (left_value > right_value) {
        return 1;
    }
    return 0;
}

static const char *engine_name(enum benchmark_engine_kind kind) {
    return kind == benchmark_engine_mylite ? "mylite" : "sqlite";
}

static const char *mode_name(enum benchmark_execution_mode mode) {
    switch (mode) {
    case benchmark_execution_prepare_each:
        return "prepare_each";
    case benchmark_execution_prepared:
        return "prepared";
    case benchmark_execution_write_rollback:
        return "write_rollback";
    }
    return "unknown";
}

static size_t account_count_for_rows(size_t row_count) {
    size_t count = row_count / rows_per_account;

    return count < minimum_account_count ? minimum_account_count : count;
}

static size_t tag_count_for_rows(size_t row_count) {
    size_t count = row_count / rows_per_account;

    if (count < minimum_tag_count) {
        return minimum_tag_count;
    }
    return count > maximum_tag_count ? maximum_tag_count : count;
}

static size_t scenario_iterations(
    const struct benchmark_scenario *scenario,
    const struct benchmark_options *options
) {
    return options->iteration_override == 0U ? scenario->default_iterations
                                             : options->iteration_override;
}

static int parse_size(const char *text, bool allow_zero, size_t *out_value) {
    char *end = NULL;
    unsigned long long value = 0ULL;

    errno = 0;
    value = strtoull(text, &end, decimal_base);
    if (errno != 0 || end == text || *end != '\0' || (!allow_zero && value == 0ULL) ||
        value > SIZE_MAX) {
        return 1;
    }
    *out_value = (size_t)value;
    return 0;
}

static const char *option_value(int argc, char **argv, int *index) {
    if (*index + 1 >= argc) {
        return NULL;
    }
    ++*index;
    return argv[*index];
}

static const char *default_database_directory(void) {
#if defined(_WIN32)
    const char *directory = getenv("TEMP");
#else
    const char *directory = getenv("TMPDIR");
#endif

    return directory == NULL || directory[0] == '\0' ? "." : directory;
}

static long benchmark_process_id(void) {
#if defined(_WIN32)
    return (long)_getpid();
#else
    return (long)getpid();
#endif
}
