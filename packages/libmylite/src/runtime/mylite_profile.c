#include "mylite_profile_internal.h"

#include "mylite_profile_allocator.h"

#include "mylite_connection.h"
#include "sqlite3.h"

#include <string.h>
#include <time.h>

#ifdef _WIN32
#  include <windows.h>
#endif

#ifdef _MSC_VER
#  define MYLITE_PROFILE_THREAD_LOCAL __declspec(thread)
#else
#  define MYLITE_PROFILE_THREAD_LOCAL _Thread_local
#endif

static uint64_t elapsed_since(uint64_t started_ns);
static void add_counter(uint64_t *counter, uint64_t value);
static void record_allocation(size_t bytes);
static int profile_sqlite3_step(sqlite3_stmt *statement, bool is_metadata);
static void record_statement_status(
    struct mylite_profile_snapshot *profile,
    sqlite3_stmt *statement,
    bool is_metadata
);
static void record_one_statement_status(
    uint64_t *total,
    uint64_t *metadata,
    sqlite3_stmt *statement,
    int operation,
    bool is_metadata
);

static MYLITE_PROFILE_THREAD_LOCAL mylite_db *active_api_database = NULL;

int mylite_profile_start(mylite_db *database) {
    if (database == NULL || database->sqlite == NULL || database->profile_active) {
        return MYLITE_MISUSE;
    }

    memset(&database->profile, 0, sizeof(database->profile));
    database->profile_active = true;
    return MYLITE_OK;
}

int mylite_profile_stop(mylite_db *database, struct mylite_profile_snapshot *out_snapshot) {
    if (database == NULL || out_snapshot == NULL || !database->profile_active) {
        return MYLITE_MISUSE;
    }

    *out_snapshot = database->profile;
    database->profile_active = false;
    if (active_api_database == database) {
        active_api_database = NULL;
    }
    return MYLITE_OK;
}

uint64_t mylite_profile_now_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;

    if (QueryPerformanceCounter(&counter) == 0 || QueryPerformanceFrequency(&frequency) == 0 ||
        frequency.QuadPart <= 0) {
        return 0U;
    }
    {
        uint64_t counter_value = (uint64_t)counter.QuadPart;
        uint64_t frequency_value = (uint64_t)frequency.QuadPart;
        uint64_t seconds = counter_value / frequency_value;
        uint64_t remainder = counter_value % frequency_value;
        uint64_t fractional_ns =
            (uint64_t)(((long double)remainder * 1000000000.0L) / (long double)frequency_value);

        return seconds * UINT64_C(1000000000) + fractional_ns;
    }
#else
    struct timespec now = {0};

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0U;
    }
    return ((uint64_t)now.tv_sec * UINT64_C(1000000000)) + (uint64_t)now.tv_nsec;
#endif
}

void *mylite_profile_malloc(size_t size) {
    void *allocation = malloc(size);

    if (allocation != NULL) {
        record_allocation(size);
    }
    return allocation;
}

void *mylite_profile_calloc(size_t count, size_t size) {
    void *allocation = calloc(count, size);

    if (allocation != NULL) {
        record_allocation(count > 0U && size > SIZE_MAX / count ? SIZE_MAX : count * size);
    }
    return allocation;
}

void *mylite_profile_realloc(void *allocation, size_t size) {
    void *resized = realloc(allocation, size);

    if (resized != NULL) {
        record_allocation(size);
    }
    return resized;
}

void mylite_profile_free(void *allocation) {
    free(allocation);
}

void mylite_profile_enter_api(mylite_db *database) {
    active_api_database = database != NULL && database->profile_active ? database : NULL;
}

void mylite_profile_leave_api(mylite_db *database) {
    if (active_api_database == database) {
        active_api_database = NULL;
    }
}

void mylite_profile_record_statement(mylite_db *database, uint64_t started_ns) {
    if (database != NULL && database->profile_active) {
        database->profile.statement_api_ns += elapsed_since(started_ns);
        ++database->profile.statement_count;
    }
    if (active_api_database == database) {
        active_api_database = NULL;
    }
}

void mylite_profile_record_normalization(mylite_db *database, uint64_t started_ns) {
    if (database != NULL && database->profile_active) {
        database->profile.normalization_ns += elapsed_since(started_ns);
        ++database->profile.normalization_count;
    }
}

void mylite_profile_record_parse(
    mylite_db *database,
    struct mylite_profile_parse_observation observation
) {
    if (database != NULL && database->profile_active) {
        database->profile.parse_ns += elapsed_since(observation.started_ns);
        ++database->profile.parse_count;
        database->profile.parser_retry_callback_count += (uint64_t)observation.retry_callback_count;
        database->profile.parser_retry_handled_count += (uint64_t)observation.retry_handled_count;
    }
}

void mylite_profile_record_select_plan(mylite_db *database, uint64_t started_ns, bool cache_hit) {
    if (database == NULL || !database->profile_active) {
        return;
    }
    if (cache_hit) {
        ++database->profile.select_plan_cache_hit_count;
        return;
    }
    database->profile.select_plan_ns += elapsed_since(started_ns);
    ++database->profile.select_plan_count;
}

void mylite_profile_record_dml_plan(mylite_db *database, uint64_t started_ns, bool cache_hit) {
    if (database == NULL || !database->profile_active) {
        return;
    }
    if (cache_hit) {
        ++database->profile.dml_plan_cache_hit_count;
        return;
    }
    database->profile.dml_plan_ns += elapsed_since(started_ns);
    ++database->profile.dml_plan_count;
}

void mylite_profile_record_select_lowering(
    mylite_db *database,
    uint64_t started_ns,
    bool cache_hit
) {
    if (database == NULL || !database->profile_active) {
        return;
    }
    if (cache_hit) {
        ++database->profile.select_lowering_cache_hit_count;
        return;
    }
    database->profile.select_lowering_ns += elapsed_since(started_ns);
    ++database->profile.select_lowering_count;
}

void mylite_profile_record_result_buffer(uint64_t started_ns, bool completed, size_t value_bytes) {
    mylite_db *database = active_api_database;

    if (database == NULL || !database->profile_active) {
        return;
    }
    database->profile.result_buffer_ns += elapsed_since(started_ns);
    if (completed) {
        ++database->profile.result_row_count;
        database->profile.result_value_bytes += (uint64_t)value_bytes;
    }
}

void mylite_profile_record_cursor_step(
    mylite_db *database,
    uint64_t started_ns,
    bool produced_row,
    size_t value_bytes
) {
    if (database == NULL || !database->profile_active) {
        return;
    }
    database->profile.cursor_step_ns += elapsed_since(started_ns);
    if (produced_row) {
        ++database->profile.cursor_row_count;
        database->profile.cursor_value_bytes += (uint64_t)value_bytes;
    }
    if (active_api_database == database) {
        active_api_database = NULL;
    }
}

void mylite_profile_record_cursor_finalize(mylite_db *database, uint64_t started_ns) {
    if (database != NULL && database->profile_active) {
        database->profile.cursor_finalize_ns += elapsed_since(started_ns);
        ++database->profile.cursor_finalize_count;
    }
    if (active_api_database == database) {
        active_api_database = NULL;
    }
}

int mylite_profile_sqlite3_step(sqlite3_stmt *statement) {
    return profile_sqlite3_step(statement, false);
}

int mylite_profile_catalog_sqlite3_step(sqlite3_stmt *statement) {
    return profile_sqlite3_step(statement, true);
}

void mylite_profile_record_scalar_callback(uint64_t started_ns) {
    mylite_db *database = active_api_database;

    if (database == NULL || !database->profile_active) {
        return;
    }
    add_counter(&database->profile.scalar_callback_ns, elapsed_since(started_ns));
    add_counter(&database->profile.scalar_callback_count, 1U);
}

void mylite_profile_record_collation_callback(uint64_t started_ns) {
    mylite_db *database = active_api_database;

    if (database == NULL || !database->profile_active) {
        return;
    }
    add_counter(&database->profile.collation_callback_ns, elapsed_since(started_ns));
    add_counter(&database->profile.collation_callback_count, 1U);
}

void mylite_profile_record_descriptor_copy(mylite_db *database, size_t bytes) {
    if (database == NULL || !database->profile_active || bytes == 0U) {
        return;
    }
    ++database->profile.descriptor_copy_count;
    database->profile.descriptor_copy_bytes += (uint64_t)bytes;
}

void mylite_profile_record_statement_cache_event(
    mylite_db *database,
    enum mylite_profile_statement_cache_kind kind,
    enum mylite_profile_statement_cache_event event
) {
    if (database == NULL || !database->profile_active) {
        return;
    }
    if (kind == MYLITE_PROFILE_EXECUTION_STATEMENT_CACHE) {
        if (event == MYLITE_PROFILE_STATEMENT_CACHE_HIT) {
            ++database->profile.execution_statement_cache_hit_count;
        } else if (event == MYLITE_PROFILE_STATEMENT_CACHE_MISS) {
            ++database->profile.execution_statement_cache_miss_count;
        } else if (event == MYLITE_PROFILE_STATEMENT_CACHE_EVICTION) {
            ++database->profile.execution_statement_cache_eviction_count;
        } else if (event == MYLITE_PROFILE_STATEMENT_CACHE_UNCACHED_PREPARE) {
            ++database->profile.execution_statement_cache_uncached_prepare_count;
        }
        return;
    }
    if (kind == MYLITE_PROFILE_CATALOG_STATEMENT_CACHE) {
        if (event == MYLITE_PROFILE_STATEMENT_CACHE_HIT) {
            ++database->profile.catalog_statement_cache_hit_count;
        } else if (event == MYLITE_PROFILE_STATEMENT_CACHE_MISS) {
            ++database->profile.catalog_statement_cache_miss_count;
        } else if (event == MYLITE_PROFILE_STATEMENT_CACHE_UNCACHED_PREPARE) {
            ++database->profile.catalog_statement_cache_uncached_prepare_count;
        }
    }
}

void mylite_profile_detach(mylite_db *database) {
    if (database == NULL || !database->profile_active) {
        return;
    }
    database->profile_active = false;
    if (active_api_database == database) {
        active_api_database = NULL;
    }
}

static void record_allocation(size_t bytes) {
    mylite_db *database = active_api_database;

    if (database == NULL || !database->profile_active) {
        return;
    }
    add_counter(&database->profile.allocation_count, 1U);
    add_counter(&database->profile.allocation_bytes, (uint64_t)bytes);
}

static int profile_sqlite3_step(sqlite3_stmt *statement, bool is_metadata) {
    mylite_db *database = active_api_database;
    uint64_t started_ns = mylite_profile_now_ns();
    int rc = sqlite3_step(statement);

    if (database != NULL && database->profile_active) {
        uint64_t elapsed_ns = elapsed_since(started_ns);

        add_counter(&database->profile.sqlite_step_ns, elapsed_ns);
        add_counter(&database->profile.sqlite_step_count, 1U);
        if (is_metadata) {
            add_counter(&database->profile.metadata_step_ns, elapsed_ns);
            add_counter(&database->profile.metadata_step_count, 1U);
        }
        record_statement_status(&database->profile, statement, is_metadata);
    }
    return rc;
}

static void add_counter(uint64_t *counter, uint64_t value) {
    if (value > UINT64_MAX - *counter) {
        *counter = UINT64_MAX;
        return;
    }
    *counter += value;
}

static void record_statement_status(
    struct mylite_profile_snapshot *profile,
    sqlite3_stmt *statement,
    bool is_metadata
) {
    record_one_statement_status(
        &profile->sqlite_vm_step_count,
        &profile->metadata_vm_step_count,
        statement,
        SQLITE_STMTSTATUS_VM_STEP,
        is_metadata
    );
    record_one_statement_status(
        &profile->sqlite_fullscan_step_count,
        &profile->metadata_fullscan_step_count,
        statement,
        SQLITE_STMTSTATUS_FULLSCAN_STEP,
        is_metadata
    );
    record_one_statement_status(
        &profile->sqlite_sort_count,
        &profile->metadata_sort_count,
        statement,
        SQLITE_STMTSTATUS_SORT,
        is_metadata
    );
    record_one_statement_status(
        &profile->sqlite_autoindex_count,
        &profile->metadata_autoindex_count,
        statement,
        SQLITE_STMTSTATUS_AUTOINDEX,
        is_metadata
    );
    record_one_statement_status(
        &profile->sqlite_reprepare_count,
        &profile->metadata_reprepare_count,
        statement,
        SQLITE_STMTSTATUS_REPREPARE,
        is_metadata
    );
    record_one_statement_status(
        &profile->sqlite_run_count,
        &profile->metadata_run_count,
        statement,
        SQLITE_STMTSTATUS_RUN,
        is_metadata
    );
    record_one_statement_status(
        &profile->sqlite_filter_hit_count,
        &profile->metadata_filter_hit_count,
        statement,
        SQLITE_STMTSTATUS_FILTER_HIT,
        is_metadata
    );
    record_one_statement_status(
        &profile->sqlite_filter_miss_count,
        &profile->metadata_filter_miss_count,
        statement,
        SQLITE_STMTSTATUS_FILTER_MISS,
        is_metadata
    );
}

static void record_one_statement_status(
    uint64_t *total,
    uint64_t *metadata,
    sqlite3_stmt *statement,
    int operation,
    bool is_metadata
) {
    int value = sqlite3_stmt_status(statement, operation, 1);

    if (value <= 0) {
        return;
    }
    add_counter(total, (uint64_t)value);
    if (is_metadata) {
        add_counter(metadata, (uint64_t)value);
    }
}

static uint64_t elapsed_since(uint64_t started_ns) {
    uint64_t now_ns = mylite_profile_now_ns();

    return now_ns >= started_ns ? now_ns - started_ns : 0U;
}
