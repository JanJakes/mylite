#ifndef MYLITE_RUNTIME_MYLITE_PROFILE_INTERNAL_H
#define MYLITE_RUNTIME_MYLITE_PROFILE_INTERNAL_H

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct sqlite3_stmt;

struct mylite_profile_parse_observation {
    uint64_t started_ns;
    size_t retry_callback_count;
    size_t retry_handled_count;
};

struct mylite_profile_snapshot {
    uint64_t statement_api_ns;
    uint64_t normalization_ns;
    uint64_t parse_ns;
    uint64_t select_plan_ns;
    uint64_t dml_plan_ns;
    uint64_t select_lowering_ns;
    uint64_t sqlite_step_ns;
    uint64_t metadata_step_ns;
    uint64_t scalar_callback_ns;
    uint64_t collation_callback_ns;
    uint64_t result_buffer_ns;
    uint64_t cursor_step_ns;
    uint64_t cursor_finalize_ns;
    uint64_t statement_count;
    uint64_t normalization_count;
    uint64_t parse_count;
    uint64_t parser_retry_callback_count;
    uint64_t parser_retry_handled_count;
    uint64_t select_plan_count;
    uint64_t select_plan_cache_hit_count;
    uint64_t dml_plan_count;
    uint64_t dml_plan_cache_hit_count;
    uint64_t select_lowering_count;
    uint64_t select_lowering_cache_hit_count;
    uint64_t sqlite_step_count;
    uint64_t metadata_step_count;
    uint64_t sqlite_vm_step_count;
    uint64_t sqlite_fullscan_step_count;
    uint64_t sqlite_sort_count;
    uint64_t sqlite_autoindex_count;
    uint64_t sqlite_reprepare_count;
    uint64_t sqlite_run_count;
    uint64_t sqlite_filter_hit_count;
    uint64_t sqlite_filter_miss_count;
    uint64_t metadata_vm_step_count;
    uint64_t metadata_fullscan_step_count;
    uint64_t metadata_sort_count;
    uint64_t metadata_autoindex_count;
    uint64_t metadata_reprepare_count;
    uint64_t metadata_run_count;
    uint64_t metadata_filter_hit_count;
    uint64_t metadata_filter_miss_count;
    uint64_t scalar_callback_count;
    uint64_t collation_callback_count;
    uint64_t allocation_count;
    uint64_t allocation_bytes;
    uint64_t descriptor_copy_count;
    uint64_t descriptor_copy_bytes;
    uint64_t execution_statement_cache_hit_count;
    uint64_t execution_statement_cache_miss_count;
    uint64_t execution_statement_cache_eviction_count;
    uint64_t execution_statement_cache_uncached_prepare_count;
    uint64_t catalog_statement_cache_hit_count;
    uint64_t catalog_statement_cache_miss_count;
    uint64_t catalog_statement_cache_uncached_prepare_count;
    uint64_t result_row_count;
    uint64_t result_value_bytes;
    uint64_t cursor_row_count;
    uint64_t cursor_value_bytes;
    uint64_t cursor_finalize_count;
};

enum mylite_profile_statement_cache_kind {
    MYLITE_PROFILE_EXECUTION_STATEMENT_CACHE = 1,
    MYLITE_PROFILE_CATALOG_STATEMENT_CACHE = 2,
};

enum mylite_profile_statement_cache_event {
    MYLITE_PROFILE_STATEMENT_CACHE_HIT = 1,
    MYLITE_PROFILE_STATEMENT_CACHE_MISS = 2,
    MYLITE_PROFILE_STATEMENT_CACHE_EVICTION = 3,
    MYLITE_PROFILE_STATEMENT_CACHE_UNCACHED_PREPARE = 4,
};

int mylite_profile_start(mylite_db *database);
int mylite_profile_stop(mylite_db *database, struct mylite_profile_snapshot *out_snapshot);
uint64_t mylite_profile_now_ns(void);
void mylite_profile_enter_api(mylite_db *database);
void mylite_profile_leave_api(mylite_db *database);
void mylite_profile_record_statement(mylite_db *database, uint64_t started_ns);
void mylite_profile_record_normalization(mylite_db *database, uint64_t started_ns);
void mylite_profile_record_parse(
    mylite_db *database,
    struct mylite_profile_parse_observation observation
);
void mylite_profile_record_select_plan(mylite_db *database, uint64_t started_ns, bool cache_hit);
void mylite_profile_record_dml_plan(mylite_db *database, uint64_t started_ns, bool cache_hit);
void mylite_profile_record_select_lowering(
    mylite_db *database,
    uint64_t started_ns,
    bool cache_hit
);
void mylite_profile_record_result_buffer(uint64_t started_ns, bool completed, size_t value_bytes);
void mylite_profile_record_cursor_step(
    mylite_db *database,
    uint64_t started_ns,
    bool produced_row,
    size_t value_bytes
);
void mylite_profile_record_cursor_finalize(mylite_db *database, uint64_t started_ns);
int mylite_profile_sqlite3_step(struct sqlite3_stmt *statement);
int mylite_profile_catalog_sqlite3_step(struct sqlite3_stmt *statement);
void mylite_profile_record_scalar_callback(uint64_t started_ns);
void mylite_profile_record_collation_callback(uint64_t started_ns);
void mylite_profile_record_descriptor_copy(mylite_db *database, size_t bytes);
void mylite_profile_record_statement_cache_event(
    mylite_db *database,
    enum mylite_profile_statement_cache_kind kind,
    enum mylite_profile_statement_cache_event event
);
void mylite_profile_detach(mylite_db *database);

#endif
