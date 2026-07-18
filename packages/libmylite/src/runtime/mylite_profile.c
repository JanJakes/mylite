#include "mylite_profile_internal.h"

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

void mylite_profile_enter_api(mylite_db *database) {
    active_api_database = database != NULL && database->profile_active ? database : NULL;
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

void mylite_profile_record_parse(mylite_db *database, uint64_t started_ns) {
    if (database != NULL && database->profile_active) {
        database->profile.parse_ns += elapsed_since(started_ns);
        ++database->profile.parse_count;
    }
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
    mylite_db *database = active_api_database;
    uint64_t started_ns = mylite_profile_now_ns();
    int rc = sqlite3_step(statement);

    if (database != NULL && database->profile_active) {
        database->profile.sqlite_step_ns += elapsed_since(started_ns);
        ++database->profile.sqlite_step_count;
    }
    return rc;
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

static uint64_t elapsed_since(uint64_t started_ns) {
    uint64_t now_ns = mylite_profile_now_ns();

    return now_ns >= started_ns ? now_ns - started_ns : 0U;
}
