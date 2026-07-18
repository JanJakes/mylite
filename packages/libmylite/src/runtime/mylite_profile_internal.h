#ifndef MYLITE_RUNTIME_MYLITE_PROFILE_INTERNAL_H
#define MYLITE_RUNTIME_MYLITE_PROFILE_INTERNAL_H

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct sqlite3_stmt;

struct mylite_profile_snapshot {
    uint64_t statement_api_ns;
    uint64_t normalization_ns;
    uint64_t parse_ns;
    uint64_t select_plan_ns;
    uint64_t select_lowering_ns;
    uint64_t sqlite_step_ns;
    uint64_t result_buffer_ns;
    uint64_t cursor_step_ns;
    uint64_t cursor_finalize_ns;
    uint64_t statement_count;
    uint64_t normalization_count;
    uint64_t parse_count;
    uint64_t select_plan_count;
    uint64_t select_plan_cache_hit_count;
    uint64_t select_lowering_count;
    uint64_t select_lowering_cache_hit_count;
    uint64_t sqlite_step_count;
    uint64_t result_row_count;
    uint64_t result_value_bytes;
    uint64_t cursor_row_count;
    uint64_t cursor_value_bytes;
    uint64_t cursor_finalize_count;
};

int mylite_profile_start(mylite_db *database);
int mylite_profile_stop(mylite_db *database, struct mylite_profile_snapshot *out_snapshot);
uint64_t mylite_profile_now_ns(void);
void mylite_profile_enter_api(mylite_db *database);
void mylite_profile_record_statement(mylite_db *database, uint64_t started_ns);
void mylite_profile_record_normalization(mylite_db *database, uint64_t started_ns);
void mylite_profile_record_parse(mylite_db *database, uint64_t started_ns);
void mylite_profile_record_select_plan(mylite_db *database, uint64_t started_ns, bool cache_hit);
void mylite_profile_record_select_lowering(mylite_db *database, uint64_t started_ns, bool cache_hit);
void mylite_profile_record_result_buffer(uint64_t started_ns, bool completed, size_t value_bytes);
void mylite_profile_record_cursor_step(
    mylite_db *database,
    uint64_t started_ns,
    bool produced_row,
    size_t value_bytes
);
void mylite_profile_record_cursor_finalize(mylite_db *database, uint64_t started_ns);
int mylite_profile_sqlite3_step(struct sqlite3_stmt *statement);
void mylite_profile_detach(mylite_db *database);

#endif
