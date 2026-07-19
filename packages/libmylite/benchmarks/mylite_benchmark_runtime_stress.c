#include "mylite_benchmark_runtime_stress.h"

#include <mylite/mylite.h>

#include "runtime/mylite_catalog_string_pool.h"
#include "runtime/mylite_connection.h"
#include "runtime/mylite_execution_loaded_catalog.h"

#include <limits.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <process.h>
#  include <windows.h>
#else
#  include <pthread.h>
#  include <time.h>
#  include <unistd.h>
#endif

enum {
    stress_path_capacity = 1024,
    stress_safe_name_capacity = 128,
    generated_sql_base_capacity = 128,
    generated_sql_item_capacity = 32,
    catalog_ddl_sql_capacity = 256,
    metadata_ddl_sql_capacity = 512,
    nanoseconds_per_second = 1000000000ULL,
    cache_table_count = 66,
    cache_warm_table_count = 64,
    processlist_worker_count = 8,
};

enum runtime_stress_kind {
    runtime_stress_cold_open,
    runtime_stress_large_in,
    runtime_stress_large_or,
    runtime_stress_scalar_projection,
    runtime_stress_grouped_projection,
    runtime_stress_wide_order,
    runtime_stress_wide_projection,
    runtime_stress_cache_saturation,
    runtime_stress_metadata_columns,
    runtime_stress_catalog_ddl_generations,
    runtime_stress_concurrent_read_write,
    runtime_stress_processlist_concurrent,
};

struct runtime_stress_scenario {
    const char *name;
    enum runtime_stress_kind kind;
    size_t scale;
};

struct runtime_stress_worker {
    mylite_db *database;
    const char *sql;
    size_t sql_length;
    size_t iterations;
    atomic_bool *start;
    size_t ok_count;
    size_t error_count;
};

struct runtime_processlist_context {
    mylite_db *observer;
    mylite_db *databases[processlist_worker_count];
    struct runtime_stress_worker workers[processlist_worker_count];
    atomic_bool start;
    size_t worker_count;
    size_t thread_count;
#if defined(_WIN32)
    HANDLE threads[processlist_worker_count];
#else
    pthread_t threads[processlist_worker_count];
    bool thread_started[processlist_worker_count];
#endif
};

struct runtime_processlist_configuration {
    size_t worker_count;
    size_t warmup_iterations;
};

struct balanced_or_frame {
    size_t first_term;
    size_t term_count;
    unsigned int state;
};

static int run_cold_open_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations, // NOLINT(bugprone-easily-swappable-parameters): measured then warmup counts.
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
);
static int run_large_in_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations,
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
);
static int run_large_or_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations,
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
);
static int run_scalar_projection_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations,
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
);
static int run_grouped_projection_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations,
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
);
static int run_wide_order_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations,
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
);
static int run_wide_projection_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations,
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
);
static int run_cache_saturation_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations, // NOLINT(bugprone-easily-swappable-parameters): measured then warmup counts.
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
);
static int run_metadata_columns_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations,
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
);
static int run_catalog_ddl_generations_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations,
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
);
static int run_catalog_ddl_generation(
    mylite_db *database,
    size_t generation,
    struct mylite_benchmark_runtime_stress_measurement *measurement
);
static int run_concurrent_read_write_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations,
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
);
static int run_processlist_concurrent_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations,
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
);
static int initialize_processlist_context(
    struct runtime_processlist_context *context,
    const struct runtime_processlist_configuration *configuration
);
static int start_processlist_workers(
    struct runtime_processlist_context *context,
    size_t iterations
);
static void deinit_processlist_context(struct runtime_processlist_context *context);
static int finish_processlist_workers(
    struct runtime_processlist_context *context,
    struct mylite_benchmark_runtime_stress_measurement *measurement
);
static int prepare_stress_database(const char *path, mylite_db **out_database);
static int execute_stress_sql(mylite_db *database, const char *sql, size_t sql_length);
static int execute_stress_iterations(
    mylite_db *database,
    const char *sql,
    size_t sql_length,
    size_t iterations,
    struct mylite_benchmark_runtime_stress_measurement *measurement
);
static char *build_large_in_query(size_t value_count, size_t *out_length);
static char *build_large_or_query(size_t term_count, size_t *out_length);
static char *build_scalar_projection_query(size_t expression_count, size_t *out_length);
static char *build_grouped_projection_query(size_t projection_count, size_t *out_length);
static char *build_wide_order_query(size_t column_count, size_t *out_length);
static bool append_balanced_or_query(
    char *sql,
    size_t capacity,
    size_t *length,
    size_t first_term,
    size_t term_count
);
static char *build_wide_table_statement(size_t column_count, size_t *out_length);
static char *build_wide_select_statement(size_t column_count, size_t *out_length);
static char *allocate_generated_sql(size_t item_count, size_t *out_capacity);
static bool append_generated_sql(char *sql, size_t capacity, size_t *length, const char *text);
static bool append_generated_indexed_sql(
    char *sql,
    size_t capacity,
    size_t *length,
    const char *format,
    size_t index
);
static int make_stress_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_stress_files(const char *path);
static void remove_stress_file_with_suffix(const char *path, const char *suffix);
static uint64_t monotonic_now_ns(void);
#if defined(_WIN32)
static DWORD WINAPI run_stress_worker(LPVOID argument);
#else
static void *run_stress_worker(void *argument);
#endif
static void execute_stress_worker(struct runtime_stress_worker *worker);

static const struct runtime_stress_scenario runtime_stress_scenarios[] = {
    {"runtime.cold_open", runtime_stress_cold_open, 0U},
    {"runtime.large_in_256", runtime_stress_large_in, 256U},
    {"runtime.large_in_4096", runtime_stress_large_in, 4096U},
    {"runtime.large_or_2048", runtime_stress_large_or, 2048U},
    {"runtime.scalar_projection_128", runtime_stress_scalar_projection, 128U},
    {"runtime.grouped_projection_128", runtime_stress_grouped_projection, 128U},
    {"runtime.wide_order_128", runtime_stress_wide_order, 128U},
    {"runtime.wide_projection_16", runtime_stress_wide_projection, 16U},
    {"runtime.wide_projection_128", runtime_stress_wide_projection, 128U},
    {"runtime.catalog_cache_saturation", runtime_stress_cache_saturation, cache_table_count},
    {"runtime.metadata_columns_128", runtime_stress_metadata_columns, 128U},
    {"runtime.catalog_ddl_generations", runtime_stress_catalog_ddl_generations, 0U},
    {"runtime.concurrent_read_write", runtime_stress_concurrent_read_write, 0U},
    {
        "runtime.processlist_concurrent_8",
        runtime_stress_processlist_concurrent,
        processlist_worker_count,
    },
};

size_t mylite_benchmark_runtime_stress_scenario_count(void) {
    return sizeof(runtime_stress_scenarios) / sizeof(runtime_stress_scenarios[0]);
}

const char *mylite_benchmark_runtime_stress_scenario_name(size_t index) {
    if (index >= mylite_benchmark_runtime_stress_scenario_count()) {
        return NULL;
    }
    return runtime_stress_scenarios[index].name;
}

int mylite_benchmark_run_runtime_stress_scenario(
    const char *name,
    size_t iterations,
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
) {
    const struct runtime_stress_scenario *scenario = NULL;

    if (name == NULL || out_measurement == NULL) {
        return 1;
    }
    *out_measurement = (struct mylite_benchmark_runtime_stress_measurement){0};
    for (size_t index = 0U; index < mylite_benchmark_runtime_stress_scenario_count(); ++index) {
        if (strcmp(runtime_stress_scenarios[index].name, name) == 0) {
            scenario = &runtime_stress_scenarios[index];
            break;
        }
    }
    if (scenario == NULL) {
        return 1;
    }

    switch (scenario->kind) {
    case runtime_stress_cold_open:
        return run_cold_open_scenario(scenario, iterations, warmup_iterations, out_measurement);
    case runtime_stress_large_in:
        return run_large_in_scenario(scenario, iterations, warmup_iterations, out_measurement);
    case runtime_stress_large_or:
        return run_large_or_scenario(scenario, iterations, warmup_iterations, out_measurement);
    case runtime_stress_scalar_projection:
        return run_scalar_projection_scenario(
            scenario,
            iterations,
            warmup_iterations,
            out_measurement
        );
    case runtime_stress_grouped_projection:
        return run_grouped_projection_scenario(
            scenario,
            iterations,
            warmup_iterations,
            out_measurement
        );
    case runtime_stress_wide_order:
        return run_wide_order_scenario(scenario, iterations, warmup_iterations, out_measurement);
    case runtime_stress_wide_projection:
        return run_wide_projection_scenario(
            scenario,
            iterations,
            warmup_iterations,
            out_measurement
        );
    case runtime_stress_cache_saturation:
        return run_cache_saturation_scenario(
            scenario,
            iterations,
            warmup_iterations,
            out_measurement
        );
    case runtime_stress_metadata_columns:
        return run_metadata_columns_scenario(
            scenario,
            iterations,
            warmup_iterations,
            out_measurement
        );
    case runtime_stress_catalog_ddl_generations:
        return run_catalog_ddl_generations_scenario(
            scenario,
            iterations,
            warmup_iterations,
            out_measurement
        );
    case runtime_stress_concurrent_read_write:
        return run_concurrent_read_write_scenario(
            scenario,
            iterations,
            warmup_iterations,
            out_measurement
        );
    case runtime_stress_processlist_concurrent:
        return run_processlist_concurrent_scenario(
            scenario,
            iterations,
            warmup_iterations,
            out_measurement
        );
    }
    return 1;
}

static int run_cold_open_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations, // NOLINT(bugprone-easily-swappable-parameters): measured then warmup counts.
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
) {
    char path[stress_path_capacity];
    mylite_db *database = NULL;
    uint64_t started = 0U;

    if (make_stress_path(path, sizeof(path), scenario->name) != 0) {
        return 1;
    }
    remove_stress_files(path);
    if (mylite_open(path, &database) != MYLITE_OK) {
        remove_stress_files(path);
        return 1;
    }
    mylite_close(database);
    for (size_t index = 0U; index < warmup_iterations; ++index) {
        if (mylite_open(path, &database) != MYLITE_OK) {
            remove_stress_files(path);
            return 1;
        }
        mylite_close(database);
    }

    started = monotonic_now_ns();
    for (size_t index = 0U; index < iterations; ++index) {
        if (mylite_open(path, &database) != MYLITE_OK) {
            ++out_measurement->error_count;
            break;
        }
        mylite_close(database);
        ++out_measurement->ok_count;
        ++out_measurement->operations;
    }
    out_measurement->elapsed_ns = monotonic_now_ns() - started;
    remove_stress_files(path);
    return out_measurement->error_count == 0U ? 0 : 1;
}

static int run_large_in_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations,
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
) {
    char path[stress_path_capacity];
    char *query = NULL;
    size_t query_length = 0U;
    mylite_db *database = NULL;
    uint64_t started = 0U;
    int rc = 1;

    if (make_stress_path(path, sizeof(path), scenario->name) != 0) {
        return 1;
    }
    remove_stress_files(path);
    query = build_large_in_query(scenario->scale, &query_length);
    if (query == NULL || prepare_stress_database(path, &database) != 0 ||
        execute_stress_sql(
            database,
            "CREATE TABLE benchmark_values (id BIGINT PRIMARY KEY)",
            sizeof("CREATE TABLE benchmark_values (id BIGINT PRIMARY KEY)") - 1U
        ) != 0 ||
        execute_stress_iterations(database, query, query_length, warmup_iterations, NULL) != 0) {
        goto cleanup;
    }

    started = monotonic_now_ns();
    rc = execute_stress_iterations(database, query, query_length, iterations, out_measurement);
    out_measurement->elapsed_ns = monotonic_now_ns() - started;

cleanup:
    free(query);
    mylite_close(database);
    remove_stress_files(path);
    return rc;
}

static int run_large_or_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations,
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
) {
    char path[stress_path_capacity];
    char *query = NULL;
    size_t query_length = 0U;
    mylite_db *database = NULL;
    uint64_t started = 0U;
    int rc = 1;

    if (make_stress_path(path, sizeof(path), scenario->name) != 0) {
        return 1;
    }
    remove_stress_files(path);
    query = build_large_or_query(scenario->scale, &query_length);
    if (query == NULL || prepare_stress_database(path, &database) != 0 ||
        execute_stress_sql(
            database,
            "CREATE TABLE predicate_values (id BIGINT PRIMARY KEY)",
            sizeof("CREATE TABLE predicate_values (id BIGINT PRIMARY KEY)") - 1U
        ) != 0 ||
        execute_stress_iterations(database, query, query_length, warmup_iterations, NULL) != 0) {
        goto cleanup;
    }

    started = monotonic_now_ns();
    rc = execute_stress_iterations(database, query, query_length, iterations, out_measurement);
    out_measurement->elapsed_ns = monotonic_now_ns() - started;

cleanup:
    free(query);
    mylite_close(database);
    remove_stress_files(path);
    return rc;
}

static int run_wide_projection_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations,
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
) {
    char path[stress_path_capacity];
    char *create_sql = NULL;
    char *select_sql = NULL;
    size_t create_length = 0U;
    size_t select_length = 0U;
    mylite_db *database = NULL;
    uint64_t started = 0U;
    int rc = 1;

    if (make_stress_path(path, sizeof(path), scenario->name) != 0) {
        return 1;
    }
    remove_stress_files(path);
    create_sql = build_wide_table_statement(scenario->scale, &create_length);
    select_sql = build_wide_select_statement(scenario->scale, &select_length);
    if (create_sql == NULL || select_sql == NULL || prepare_stress_database(path, &database) != 0 ||
        execute_stress_sql(database, create_sql, create_length) != 0 ||
        execute_stress_iterations(database, select_sql, select_length, warmup_iterations, NULL) !=
            0) {
        goto cleanup;
    }

    started = monotonic_now_ns();
    rc =
        execute_stress_iterations(database, select_sql, select_length, iterations, out_measurement);
    out_measurement->elapsed_ns = monotonic_now_ns() - started;

cleanup:
    free(select_sql);
    free(create_sql);
    mylite_close(database);
    remove_stress_files(path);
    return rc;
}

static int run_scalar_projection_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations,
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
) {
    char path[stress_path_capacity];
    char *query = NULL;
    size_t query_length = 0U;
    mylite_db *database = NULL;
    uint64_t started = 0U;
    int rc = 1;

    if (make_stress_path(path, sizeof(path), scenario->name) != 0) {
        return 1;
    }
    remove_stress_files(path);
    query = build_scalar_projection_query(scenario->scale, &query_length);
    if (query == NULL || prepare_stress_database(path, &database) != 0 ||
        execute_stress_sql(
            database,
            "CREATE TABLE scalar_values (name VARCHAR(64))",
            sizeof("CREATE TABLE scalar_values (name VARCHAR(64))") - 1U
        ) != 0 ||
        execute_stress_iterations(database, query, query_length, warmup_iterations, NULL) != 0) {
        goto cleanup;
    }

    started = monotonic_now_ns();
    rc = execute_stress_iterations(database, query, query_length, iterations, out_measurement);
    out_measurement->elapsed_ns = monotonic_now_ns() - started;

cleanup:
    free(query);
    mylite_close(database);
    remove_stress_files(path);
    return rc;
}

static int run_grouped_projection_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations,
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
) {
    char path[stress_path_capacity];
    char *create_sql = NULL;
    char *select_sql = NULL;
    size_t create_length = 0U;
    size_t select_length = 0U;
    mylite_db *database = NULL;
    uint64_t started = 0U;
    int rc = 1;

    if (make_stress_path(path, sizeof(path), scenario->name) != 0) {
        return 1;
    }
    remove_stress_files(path);
    create_sql = build_wide_table_statement(scenario->scale, &create_length);
    select_sql = build_grouped_projection_query(scenario->scale, &select_length);
    if (create_sql == NULL || select_sql == NULL || prepare_stress_database(path, &database) != 0 ||
        execute_stress_sql(database, create_sql, create_length) != 0 ||
        execute_stress_iterations(database, select_sql, select_length, warmup_iterations, NULL) !=
            0) {
        goto cleanup;
    }

    started = monotonic_now_ns();
    rc =
        execute_stress_iterations(database, select_sql, select_length, iterations, out_measurement);
    out_measurement->elapsed_ns = monotonic_now_ns() - started;

cleanup:
    free(select_sql);
    free(create_sql);
    mylite_close(database);
    remove_stress_files(path);
    return rc;
}

static int run_wide_order_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations,
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
) {
    char path[stress_path_capacity];
    char *create_sql = NULL;
    char *select_sql = NULL;
    size_t create_length = 0U;
    size_t select_length = 0U;
    mylite_db *database = NULL;
    uint64_t started = 0U;
    int rc = 1;

    if (make_stress_path(path, sizeof(path), scenario->name) != 0) {
        return 1;
    }
    remove_stress_files(path);
    create_sql = build_wide_table_statement(scenario->scale, &create_length);
    select_sql = build_wide_order_query(scenario->scale, &select_length);
    if (create_sql == NULL || select_sql == NULL || prepare_stress_database(path, &database) != 0 ||
        execute_stress_sql(database, create_sql, create_length) != 0 ||
        execute_stress_iterations(database, select_sql, select_length, warmup_iterations, NULL) !=
            0) {
        goto cleanup;
    }

    started = monotonic_now_ns();
    rc =
        execute_stress_iterations(database, select_sql, select_length, iterations, out_measurement);
    out_measurement->elapsed_ns = monotonic_now_ns() - started;

cleanup:
    free(select_sql);
    free(create_sql);
    mylite_close(database);
    remove_stress_files(path);
    return rc;
}

static int run_cache_saturation_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations, // NOLINT(bugprone-easily-swappable-parameters): measured then warmup counts.
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
) {
    char path[stress_path_capacity];
    char sql[generated_sql_base_capacity];
    mylite_db *database = NULL;
    uint64_t started = 0U;
    int rc = 1;

    if (make_stress_path(path, sizeof(path), scenario->name) != 0) {
        return 1;
    }
    remove_stress_files(path);
    if (prepare_stress_database(path, &database) != 0) {
        goto cleanup;
    }
    for (size_t index = 0U; index < scenario->scale; ++index) {
        int length = snprintf(sql, sizeof(sql), "CREATE TABLE cache_table_%zu (id BIGINT)", index);

        if (length < 0 || (size_t)length >= sizeof(sql) ||
            execute_stress_sql(database, sql, (size_t)length) != 0) {
            goto cleanup;
        }
    }
    for (size_t index = 0U; index < cache_warm_table_count; ++index) {
        int length = snprintf(sql, sizeof(sql), "SELECT id FROM cache_table_%zu", index);

        if (length < 0 || (size_t)length >= sizeof(sql) ||
            execute_stress_sql(database, sql, (size_t)length) != 0) {
            goto cleanup;
        }
    }
    for (size_t index = 0U; index < warmup_iterations; ++index) {
        size_t table_index = cache_warm_table_count + (index % 2U);
        int length = snprintf(sql, sizeof(sql), "SELECT id FROM cache_table_%zu", table_index);

        if (length < 0 || (size_t)length >= sizeof(sql) ||
            execute_stress_sql(database, sql, (size_t)length) != 0) {
            goto cleanup;
        }
    }

    started = monotonic_now_ns();
    for (size_t index = 0U; index < iterations; ++index) {
        size_t table_index = cache_warm_table_count + (index % 2U);
        int length = snprintf(sql, sizeof(sql), "SELECT id FROM cache_table_%zu", table_index);

        if (length < 0 || (size_t)length >= sizeof(sql) ||
            execute_stress_sql(database, sql, (size_t)length) != 0) {
            ++out_measurement->error_count;
            goto cleanup;
        }
        ++out_measurement->operations;
        ++out_measurement->ok_count;
        out_measurement->bytes += (size_t)length;
    }
    out_measurement->elapsed_ns = monotonic_now_ns() - started;
    rc = 0;

cleanup:
    mylite_close(database);
    remove_stress_files(path);
    return rc;
}

static int run_metadata_columns_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations, // NOLINT(bugprone-easily-swappable-parameters): measured then warmup counts.
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
) {
    static const char query[] =
        "SELECT TABLE_NAME, COLUMN_NAME, COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = 'benchmark' ORDER BY TABLE_NAME, ORDINAL_POSITION";
    char path[stress_path_capacity];
    char sql[metadata_ddl_sql_capacity];
    mylite_db *database = NULL;
    uint64_t started = 0U;
    int rc = 1;

    if (make_stress_path(path, sizeof(path), scenario->name) != 0) {
        return 1;
    }
    remove_stress_files(path);
    if (prepare_stress_database(path, &database) != 0) {
        goto cleanup;
    }
    for (size_t index = 0U; index < scenario->scale; ++index) {
        int length = snprintf(
            sql,
            sizeof(sql),
            "CREATE TABLE metadata_table_%zu ("
            "id BIGINT PRIMARY KEY, c1 VARCHAR(32), c2 BIGINT, c3 DATETIME, "
            "c4 DECIMAL(20,4), c5 TEXT, c6 BLOB, c7 VARCHAR(64))",
            index
        );

        if (length < 0 || (size_t)length >= sizeof(sql) ||
            execute_stress_sql(database, sql, (size_t)length) != 0) {
            goto cleanup;
        }
    }
    if (execute_stress_iterations(database, query, sizeof(query) - 1U, warmup_iterations, NULL) !=
        0) {
        goto cleanup;
    }

    started = monotonic_now_ns();
    if (execute_stress_iterations(
            database,
            query,
            sizeof(query) - 1U,
            iterations,
            out_measurement
        ) != 0) {
        goto cleanup;
    }
    out_measurement->elapsed_ns = monotonic_now_ns() - started;
    rc = 0;

cleanup:
    mylite_close(database);
    remove_stress_files(path);
    return rc;
}

static int run_catalog_ddl_generations_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations, // NOLINT(bugprone-easily-swappable-parameters): measured then warmup counts.
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
) {
    char path[stress_path_capacity];
    mylite_db *database = NULL;
    uint64_t started = 0U;
    int rc = 1;

    if (make_stress_path(path, sizeof(path), scenario->name) != 0) {
        return 1;
    }
    remove_stress_files(path);
    if (prepare_stress_database(path, &database) != 0 ||
        execute_stress_sql(
            database,
            "CREATE TABLE catalog_generations (id BIGINT, value VARCHAR(32) "
            "COMMENT 'generation-initial')",
            sizeof("CREATE TABLE catalog_generations (id BIGINT, value VARCHAR(32) "
                   "COMMENT 'generation-initial')"
            ) - 1U
        ) != 0) {
        goto cleanup;
    }
    for (size_t index = 0U; index < warmup_iterations; ++index) {
        if (run_catalog_ddl_generation(database, index, NULL) != 0) {
            goto cleanup;
        }
    }

    started = monotonic_now_ns();
    for (size_t index = 0U; index < iterations; ++index) {
        if (run_catalog_ddl_generation(database, warmup_iterations + index, out_measurement) != 0) {
            ++out_measurement->error_count;
            goto cleanup;
        }
    }
    out_measurement->elapsed_ns = monotonic_now_ns() - started;
    rc = 0;

cleanup:
    mylite_close(database);
    remove_stress_files(path);
    return rc;
}

static int run_catalog_ddl_generation(
    mylite_db *database,
    size_t generation,
    struct mylite_benchmark_runtime_stress_measurement *measurement
) {
    static const char select_sql[] = "SELECT value FROM catalog_generations LIMIT 0";
    char ddl_sql[catalog_ddl_sql_capacity];
    size_t retained_bytes = 0U;
    int ddl_length = snprintf(
        ddl_sql,
        sizeof(ddl_sql),
        "ALTER TABLE catalog_generations MODIFY COLUMN value VARCHAR(32) COMMENT "
        "'generation-%06zu-abcdefghijklmnopqrstuvwxyz0123456789'",
        generation
    );

    if (ddl_length < 0 || (size_t)ddl_length >= sizeof(ddl_sql) ||
        execute_stress_sql(database, ddl_sql, (size_t)ddl_length) != 0 ||
        execute_stress_sql(database, select_sql, sizeof(select_sql) - 1U) != 0) {
        return 1;
    }
    retained_bytes = mylite_catalog_string_pool_byte_count(&database->catalog_strings);
    for (size_t index = 0U; index < database->table_columns_cache_count; ++index) {
        retained_bytes += database->table_columns_cache[index].byte_count;
    }
    if (retained_bytes > MYLITE_EXECUTION_TABLE_COLUMNS_CACHE_BYTE_LIMIT ||
        mylite_catalog_string_pool_generation_count(&database->catalog_strings) > 1U) {
        fprintf(
            stderr,
            "catalog generation memory is unbounded: bytes=%zu generations=%zu\n",
            retained_bytes,
            mylite_catalog_string_pool_generation_count(&database->catalog_strings)
        );
        return 1;
    }
    if (measurement != NULL) {
        ++measurement->operations;
        ++measurement->ok_count;
        measurement->bytes += (size_t)ddl_length + sizeof(select_sql) - 1U;
        if (retained_bytes > measurement->peak_retained_bytes) {
            measurement->peak_retained_bytes = retained_bytes;
        }
    }
    return 0;
}

static int run_concurrent_read_write_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations, // NOLINT(bugprone-easily-swappable-parameters): measured then warmup counts.
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
) {
    static const char reader_sql[] = "SELECT value FROM benchmark_counter WHERE id = 1";
    static const char writer_sql[] = "UPDATE benchmark_counter SET value = 1 WHERE id = 1";
    char path[stress_path_capacity];
    mylite_db *setup = NULL;
    mylite_db *reader = NULL;
    mylite_db *writer = NULL;
    atomic_bool start = false;
    struct runtime_stress_worker reader_worker = {0};
    struct runtime_stress_worker writer_worker = {0};
    uint64_t started = 0U;
    int rc = 1;
#if defined(_WIN32)
    HANDLE reader_thread = NULL;
    HANDLE writer_thread = NULL;
#else
    pthread_t reader_thread;
    pthread_t writer_thread;
    bool reader_started = false;
#endif

    if (make_stress_path(path, sizeof(path), scenario->name) != 0) {
        return 1;
    }
    remove_stress_files(path);
    if (prepare_stress_database(path, &setup) != 0 ||
        execute_stress_sql(
            setup,
            "CREATE TABLE benchmark_counter (id BIGINT PRIMARY KEY, value BIGINT)",
            sizeof("CREATE TABLE benchmark_counter (id BIGINT PRIMARY KEY, value BIGINT)") - 1U
        ) != 0 ||
        execute_stress_sql(
            setup,
            "INSERT INTO benchmark_counter VALUES (1, 0)",
            sizeof("INSERT INTO benchmark_counter VALUES (1, 0)") - 1U
        ) != 0) {
        goto cleanup;
    }
    mylite_close(setup);
    setup = NULL;
    if (mylite_open(path, &reader) != MYLITE_OK || mylite_open(path, &writer) != MYLITE_OK ||
        execute_stress_sql(reader, "USE benchmark", sizeof("USE benchmark") - 1U) != 0 ||
        execute_stress_sql(writer, "USE benchmark", sizeof("USE benchmark") - 1U) != 0) {
        goto cleanup;
    }
    for (size_t index = 0U; index < warmup_iterations; ++index) {
        if (execute_stress_sql(reader, reader_sql, sizeof(reader_sql) - 1U) != 0 ||
            execute_stress_sql(writer, writer_sql, sizeof(writer_sql) - 1U) != 0) {
            goto cleanup;
        }
    }

    reader_worker = (struct runtime_stress_worker){
        .database = reader,
        .sql = reader_sql,
        .sql_length = sizeof(reader_sql) - 1U,
        .iterations = iterations,
        .start = &start,
    };
    writer_worker = (struct runtime_stress_worker){
        .database = writer,
        .sql = writer_sql,
        .sql_length = sizeof(writer_sql) - 1U,
        .iterations = iterations,
        .start = &start,
    };
#if defined(_WIN32)
    reader_thread = CreateThread(NULL, 0U, run_stress_worker, &reader_worker, 0U, NULL);
    writer_thread = CreateThread(NULL, 0U, run_stress_worker, &writer_worker, 0U, NULL);
    if (reader_thread == NULL || writer_thread == NULL) {
        atomic_store_explicit(&start, true, memory_order_release);
        goto cleanup;
    }
#else
    if (pthread_create(&reader_thread, NULL, run_stress_worker, &reader_worker) != 0) {
        goto cleanup;
    }
    reader_started = true;
    if (pthread_create(&writer_thread, NULL, run_stress_worker, &writer_worker) != 0) {
        atomic_store_explicit(&start, true, memory_order_release);
        goto cleanup;
    }
#endif
    started = monotonic_now_ns();
    atomic_store_explicit(&start, true, memory_order_release);
#if defined(_WIN32)
    (void)WaitForSingleObject(reader_thread, INFINITE);
    (void)WaitForSingleObject(writer_thread, INFINITE);
    (void)CloseHandle(reader_thread);
    (void)CloseHandle(writer_thread);
    reader_thread = NULL;
    writer_thread = NULL;
#else
    (void)pthread_join(reader_thread, NULL);
    reader_started = false;
    (void)pthread_join(writer_thread, NULL);
#endif
    out_measurement->elapsed_ns = monotonic_now_ns() - started;
    out_measurement->ok_count = reader_worker.ok_count + writer_worker.ok_count;
    out_measurement->error_count = reader_worker.error_count + writer_worker.error_count;
    out_measurement->operations = out_measurement->ok_count + out_measurement->error_count;
    out_measurement->bytes = reader_worker.ok_count * reader_worker.sql_length +
                             writer_worker.ok_count * writer_worker.sql_length;
    rc = out_measurement->error_count == 0U ? 0 : 1;
    if (reader_worker.error_count != 0U) {
        fprintf(
            stderr,
            "%s: reader errors=%zu code=%d state=%s message=%s\n",
            scenario->name,
            reader_worker.error_count,
            mylite_errcode(reader),
            mylite_sqlstate(reader),
            mylite_errmsg(reader)
        );
    }
    if (writer_worker.error_count != 0U) {
        fprintf(
            stderr,
            "%s: writer errors=%zu code=%d state=%s message=%s\n",
            scenario->name,
            writer_worker.error_count,
            mylite_errcode(writer),
            mylite_sqlstate(writer),
            mylite_errmsg(writer)
        );
    }

cleanup:
    atomic_store_explicit(&start, true, memory_order_release);
#if defined(_WIN32)
    if (reader_thread != NULL) {
        (void)WaitForSingleObject(reader_thread, INFINITE);
        (void)CloseHandle(reader_thread);
    }
    if (writer_thread != NULL) {
        (void)WaitForSingleObject(writer_thread, INFINITE);
        (void)CloseHandle(writer_thread);
    }
#else
    if (reader_started) {
        (void)pthread_join(reader_thread, NULL);
    }
#endif
    mylite_close(writer);
    mylite_close(reader);
    mylite_close(setup);
    remove_stress_files(path);
    return rc;
}

static int run_processlist_concurrent_scenario(
    const struct runtime_stress_scenario *scenario,
    size_t iterations, // NOLINT(bugprone-easily-swappable-parameters): measured then warmup counts.
    size_t warmup_iterations,
    struct mylite_benchmark_runtime_stress_measurement *out_measurement
) {
    static const char observer_sql[] = "SHOW PROCESSLIST";
    struct runtime_processlist_context context = {0};
    uint64_t started = 0U;
    int rc = 1;

    atomic_init(&context.start, false);
    if (scenario->scale != processlist_worker_count ||
        initialize_processlist_context(
            &context,
            &(const struct runtime_processlist_configuration){
                .worker_count = scenario->scale,
                .warmup_iterations = warmup_iterations,
            }
        ) != 0 ||
        start_processlist_workers(&context, iterations) != 0) {
        goto cleanup;
    }

    started = monotonic_now_ns();
    atomic_store_explicit(&context.start, true, memory_order_release);
    rc = execute_stress_iterations(
        context.observer,
        observer_sql,
        sizeof(observer_sql) - 1U,
        iterations,
        out_measurement
    );
    rc |= finish_processlist_workers(&context, out_measurement);
    out_measurement->elapsed_ns = monotonic_now_ns() - started;
    if (out_measurement->error_count != 0U) {
        rc = 1;
    }

cleanup:
    deinit_processlist_context(&context);
    return rc;
}

static int initialize_processlist_context(
    struct runtime_processlist_context *context,
    const struct runtime_processlist_configuration *configuration
) {
    static const char observer_sql[] = "SHOW PROCESSLIST";
    static const char worker_sql[] = "USE information_schema";

    context->worker_count = configuration->worker_count;
    if (mylite_open_memory(&context->observer) != MYLITE_OK) {
        return 1;
    }
    for (size_t index = 0U; index < context->worker_count; ++index) {
        if (mylite_open_memory(&context->databases[index]) != MYLITE_OK) {
            return 1;
        }
    }
    for (size_t index = 0U; index < configuration->warmup_iterations; ++index) {
        if (execute_stress_sql(context->observer, observer_sql, sizeof(observer_sql) - 1U) != 0) {
            return 1;
        }
        for (size_t worker_index = 0U; worker_index < context->worker_count; ++worker_index) {
            if (execute_stress_sql(
                    context->databases[worker_index],
                    worker_sql,
                    sizeof(worker_sql) - 1U
                ) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int start_processlist_workers(
    struct runtime_processlist_context *context,
    size_t iterations
) {
    static const char worker_sql[] = "USE information_schema";

    for (size_t index = 0U; index < context->worker_count; ++index) {
        context->workers[index] = (struct runtime_stress_worker){
            .database = context->databases[index],
            .sql = worker_sql,
            .sql_length = sizeof(worker_sql) - 1U,
            .iterations = iterations,
            .start = &context->start,
        };
#if defined(_WIN32)
        context->threads[index] =
            CreateThread(NULL, 0U, run_stress_worker, &context->workers[index], 0U, NULL);
        if (context->threads[index] == NULL) {
            return 1;
        }
#else
        if (pthread_create(
                &context->threads[index],
                NULL,
                run_stress_worker,
                &context->workers[index]
            ) != 0) {
            return 1;
        }
        context->thread_started[index] = true;
#endif
        ++context->thread_count;
    }
    return 0;
}

static void deinit_processlist_context(struct runtime_processlist_context *context) {
    atomic_store_explicit(&context->start, true, memory_order_release);
    (void)finish_processlist_workers(context, NULL);
    for (size_t index = 0U; index < context->worker_count; ++index) {
        mylite_close(context->databases[index]);
    }
    mylite_close(context->observer);
}

static int finish_processlist_workers(
    struct runtime_processlist_context *context,
    struct mylite_benchmark_runtime_stress_measurement *measurement
) {
    int rc = 0;

    for (size_t index = 0U; index < context->thread_count; ++index) {
#if defined(_WIN32)
        if (WaitForSingleObject(context->threads[index], INFINITE) != WAIT_OBJECT_0) {
            rc = 1;
        }
        (void)CloseHandle(context->threads[index]);
        context->threads[index] = NULL;
#else
        if (context->thread_started[index] && pthread_join(context->threads[index], NULL) != 0) {
            rc = 1;
        }
        context->thread_started[index] = false;
#endif
        if (measurement != NULL) {
            measurement->operations +=
                context->workers[index].ok_count + context->workers[index].error_count;
            measurement->ok_count += context->workers[index].ok_count;
            measurement->error_count += context->workers[index].error_count;
            measurement->bytes +=
                context->workers[index].ok_count * context->workers[index].sql_length;
        }
    }
    context->thread_count = 0U;
    return rc;
}

static int prepare_stress_database(const char *path, mylite_db **out_database) {
    mylite_db *database = NULL;

    if (mylite_open(path, &database) != MYLITE_OK ||
        execute_stress_sql(
            database,
            "CREATE DATABASE benchmark",
            sizeof("CREATE DATABASE benchmark") - 1U
        ) != 0 ||
        execute_stress_sql(database, "USE benchmark", sizeof("USE benchmark") - 1U) != 0) {
        mylite_close(database);
        return 1;
    }
    *out_database = database;
    return 0;
}

static int execute_stress_sql(mylite_db *database, const char *sql, size_t sql_length) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, sql_length, &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "stress query failed: rc=%d err=%d state=%s message=%s sql=%.*s\n",
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database),
            (int)sql_length,
            sql
        );
    }
    mylite_result_free(result);
    return rc == MYLITE_OK ? 0 : 1;
}

static int execute_stress_iterations(
    mylite_db *database,
    const char *sql,
    size_t sql_length, // NOLINT(bugprone-easily-swappable-parameters): SQL span then repeat count.
    size_t iterations,
    struct mylite_benchmark_runtime_stress_measurement *measurement
) {
    for (size_t index = 0U; index < iterations; ++index) {
        if (execute_stress_sql(database, sql, sql_length) != 0) {
            if (measurement != NULL) {
                ++measurement->error_count;
            }
            return 1;
        }
        if (measurement != NULL) {
            ++measurement->operations;
            ++measurement->ok_count;
            measurement->bytes += sql_length;
        }
    }
    return 0;
}

static char *build_large_in_query(size_t value_count, size_t *out_length) {
    size_t capacity = 0U;
    size_t length = 0U;
    char *sql = allocate_generated_sql(value_count, &capacity);

    if (sql == NULL || !append_generated_sql(
                           sql,
                           capacity,
                           &length,
                           "SELECT id FROM benchmark_values WHERE id IN ("
                       )) {
        free(sql);
        return NULL;
    }
    for (size_t index = 0U; index < value_count; ++index) {
        if ((index != 0U && !append_generated_sql(sql, capacity, &length, ",")) ||
            !append_generated_indexed_sql(sql, capacity, &length, "%zu", index)) {
            free(sql);
            return NULL;
        }
    }
    if (!append_generated_sql(sql, capacity, &length, ")")) {
        free(sql);
        return NULL;
    }
    *out_length = length;
    return sql;
}

static char *build_large_or_query(size_t term_count, size_t *out_length) {
    size_t capacity = 0U;
    size_t length = 0U;
    char *sql = allocate_generated_sql(term_count, &capacity);

    if (sql == NULL ||
        !append_generated_sql(sql, capacity, &length, "SELECT id FROM predicate_values WHERE ")) {
        free(sql);
        return NULL;
    }
    if (!append_balanced_or_query(sql, capacity, &length, 0U, term_count)) {
        free(sql);
        return NULL;
    }
    *out_length = length;
    return sql;
}

static bool append_balanced_or_query(
    char *sql,
    size_t capacity,
    size_t *length,
    size_t first_term,
    size_t term_count
) {
    struct balanced_or_frame stack[(sizeof(size_t) * CHAR_BIT) + 1U];
    size_t stack_count = 0U;

    if (term_count == 0U) {
        return false;
    }
    stack[stack_count++] = (struct balanced_or_frame){
        .first_term = first_term,
        .term_count = term_count,
    };
    while (stack_count != 0U) {
        struct balanced_or_frame *frame = &stack[stack_count - 1U];
        size_t left_count = frame->term_count / 2U;

        if (frame->term_count == 1U) {
            if (!append_generated_indexed_sql(
                    sql,
                    capacity,
                    length,
                    "id = %zu",
                    frame->first_term
                )) {
                return false;
            }
            --stack_count;
        } else if (frame->state == 0U) {
            if (!append_generated_sql(sql, capacity, length, "(")) {
                return false;
            }
            frame->state = 1U;
            stack[stack_count++] = (struct balanced_or_frame){
                .first_term = frame->first_term,
                .term_count = left_count,
            };
        } else if (frame->state == 1U) {
            if (!append_generated_sql(sql, capacity, length, " OR ")) {
                return false;
            }
            frame->state = 2U;
            stack[stack_count++] = (struct balanced_or_frame){
                .first_term = frame->first_term + left_count,
                .term_count = frame->term_count - left_count,
            };
        } else {
            if (!append_generated_sql(sql, capacity, length, ")")) {
                return false;
            }
            --stack_count;
        }
    }
    return true;
}

static char *build_scalar_projection_query(size_t expression_count, size_t *out_length) {
    size_t capacity = 0U;
    size_t length = 0U;
    char *sql = allocate_generated_sql(expression_count, &capacity);

    if (sql == NULL || !append_generated_sql(sql, capacity, &length, "SELECT ")) {
        free(sql);
        return NULL;
    }
    for (size_t index = 0U; index < expression_count; ++index) {
        if ((index != 0U && !append_generated_sql(sql, capacity, &length, ",")) ||
            !append_generated_indexed_sql(
                sql,
                capacity,
                &length,
                "CONCAT(name,'x') AS value_%zu",
                index
            )) {
            free(sql);
            return NULL;
        }
    }
    if (!append_generated_sql(sql, capacity, &length, " FROM scalar_values")) {
        free(sql);
        return NULL;
    }
    *out_length = length;
    return sql;
}

static char *build_grouped_projection_query(size_t projection_count, size_t *out_length) {
    size_t capacity = 0U;
    size_t length = 0U;
    char *sql = allocate_generated_sql(projection_count, &capacity);

    if (sql == NULL || !append_generated_sql(sql, capacity, &length, "SELECT ")) {
        free(sql);
        return NULL;
    }
    for (size_t index = 0U; index < projection_count; ++index) {
        if ((index != 0U && !append_generated_sql(sql, capacity, &length, ",")) ||
            !append_generated_indexed_sql(
                sql,
                capacity,
                &length,
                "column_0 AS projection_%zu",
                index
            )) {
            free(sql);
            return NULL;
        }
    }
    if (!append_generated_sql(
            sql,
            capacity,
            &length,
            ",COUNT(*) AS aggregate_count FROM wide_table GROUP BY column_0"
        )) {
        free(sql);
        return NULL;
    }
    *out_length = length;
    return sql;
}

static char *build_wide_table_statement(size_t column_count, size_t *out_length) {
    size_t capacity = 0U;
    size_t length = 0U;
    char *sql = allocate_generated_sql(column_count, &capacity);

    if (sql == NULL || !append_generated_sql(sql, capacity, &length, "CREATE TABLE wide_table (")) {
        free(sql);
        return NULL;
    }
    for (size_t index = 0U; index < column_count; ++index) {
        if ((index != 0U && !append_generated_sql(sql, capacity, &length, ",")) ||
            !append_generated_indexed_sql(sql, capacity, &length, "column_%zu BIGINT", index)) {
            free(sql);
            return NULL;
        }
    }
    if (!append_generated_sql(sql, capacity, &length, ")")) {
        free(sql);
        return NULL;
    }
    *out_length = length;
    return sql;
}

static char *build_wide_order_query(size_t column_count, size_t *out_length) {
    size_t capacity = 0U;
    size_t length = 0U;
    char *sql = allocate_generated_sql(column_count, &capacity);

    if (sql == NULL || !append_generated_sql(
                           sql,
                           capacity,
                           &length,
                           "SELECT column_0 FROM wide_table ORDER BY "
                       )) {
        free(sql);
        return NULL;
    }
    for (size_t index = 0U; index < column_count; ++index) {
        if ((index != 0U && !append_generated_sql(sql, capacity, &length, ",")) ||
            !append_generated_indexed_sql(sql, capacity, &length, "column_%zu", index)) {
            free(sql);
            return NULL;
        }
    }
    *out_length = length;
    return sql;
}

static char *build_wide_select_statement(size_t column_count, size_t *out_length) {
    (void)column_count;
    char *sql = (char *)malloc(sizeof("SELECT * FROM wide_table"));

    if (sql == NULL) {
        return NULL;
    }
    memcpy(sql, "SELECT * FROM wide_table", sizeof("SELECT * FROM wide_table"));
    *out_length = sizeof("SELECT * FROM wide_table") - 1U;
    return sql;
}

static char *allocate_generated_sql(size_t item_count, size_t *out_capacity) {
    size_t capacity = 0U;

    if (item_count > (SIZE_MAX - generated_sql_base_capacity) / generated_sql_item_capacity) {
        return NULL;
    }
    capacity = generated_sql_base_capacity + item_count * generated_sql_item_capacity;
    *out_capacity = capacity;
    return (char *)calloc(capacity, 1U);
}

static bool append_generated_sql(char *sql, size_t capacity, size_t *length, const char *text) {
    size_t text_length = strlen(text);

    if (text_length >= capacity || *length > capacity - text_length - 1U) {
        return false;
    }
    memcpy(sql + *length, text, text_length);
    *length += text_length;
    sql[*length] = '\0';
    return true;
}

static bool append_generated_indexed_sql(
    char *sql,
    size_t capacity,
    size_t *length,
    const char *format,
    size_t index
) {
    int written = 0;

    if (*length >= capacity) {
        return false;
    }
    written = snprintf(sql + *length, capacity - *length, format, index);
    if (written < 0 || (size_t)written >= capacity - *length) {
        return false;
    }
    *length += (size_t)written;
    return true;
}

static int make_stress_path(char *path, size_t path_size, const char *name) {
    char safe_name[stress_safe_name_capacity];
    size_t name_length = strlen(name);
    int written = 0;

    if (name_length >= sizeof(safe_name)) {
        return 1;
    }
    for (size_t index = 0U; index <= name_length; ++index) {
        char byte = name[index];

        if (byte == '.') {
            byte = '_';
        }
        safe_name[index] = byte;
    }
    written =
        snprintf(path, path_size, "mylite_benchmark_%d_%s.mylite", current_process_id(), safe_name);
    return written < 0 || (size_t)written >= path_size ? 1 : 0;
}

static int current_process_id(void) {
#if defined(_WIN32)
    return _getpid();
#else
    return (int)getpid();
#endif
}

static void remove_stress_files(const char *path) {
    (void)remove(path);
    remove_stress_file_with_suffix(path, "-journal");
    remove_stress_file_with_suffix(path, "-wal");
    remove_stress_file_with_suffix(path, "-shm");
}

static void remove_stress_file_with_suffix(const char *path, const char *suffix) {
    char related_path[stress_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(related_path)) {
        (void)remove(related_path);
    }
}

static uint64_t monotonic_now_ns(void) {
#if defined(_WIN32)
    LARGE_INTEGER frequency = {0};
    LARGE_INTEGER counter = {0};

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * (LONGLONG)nanoseconds_per_second) / frequency.QuadPart);
#elif defined(CLOCK_MONOTONIC)
    struct timespec timestamp = {0};

    clock_gettime(CLOCK_MONOTONIC, &timestamp);
    return ((uint64_t)timestamp.tv_sec * nanoseconds_per_second) + (uint64_t)timestamp.tv_nsec;
#else
    struct timespec timestamp = {0};

    timespec_get(&timestamp, TIME_UTC);
    return ((uint64_t)timestamp.tv_sec * nanoseconds_per_second) + (uint64_t)timestamp.tv_nsec;
#endif
}

#if defined(_WIN32)
static DWORD WINAPI run_stress_worker(LPVOID argument) {
    execute_stress_worker((struct runtime_stress_worker *)argument);
    return 0U;
}
#else
static void *run_stress_worker(void *argument) {
    execute_stress_worker((struct runtime_stress_worker *)argument);
    return NULL;
}
#endif

static void execute_stress_worker(struct runtime_stress_worker *worker) {
    while (!atomic_load_explicit(worker->start, memory_order_acquire)) {}
    for (size_t index = 0U; index < worker->iterations; ++index) {
        if (execute_stress_sql(worker->database, worker->sql, worker->sql_length) == 0) {
            ++worker->ok_count;
        } else {
            ++worker->error_count;
        }
    }
}
