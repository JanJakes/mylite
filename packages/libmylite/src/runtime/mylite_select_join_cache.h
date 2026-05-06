#ifndef MYLITE_RUNTIME_MYLITE_SELECT_JOIN_CACHE_H
#define MYLITE_RUNTIME_MYLITE_SELECT_JOIN_CACHE_H

#include <mylite/mylite.h>

#include "mylite_select_types.h"

#include <stdbool.h>

bool mylite_select_join_cache_stage_range(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    size_t available_table_count,
    struct mylite_select_table_range *out_range
);
int mylite_select_join_cache_lookup_scan(
    const struct mylite_table_select_join_condition_cache *cache,
    const struct mylite_table_select_join_scan_state *scan,
    struct mylite_select_table_range range,
    struct mylite_table_select_join_condition_cache_lookup *out_lookup
);
int mylite_select_join_cache_lookup_row(
    const struct mylite_table_select_join_condition_cache *cache,
    const struct mylite_table_select_row *row,
    struct mylite_select_table_range range,
    struct mylite_table_select_join_condition_cache_lookup *out_lookup
);
int mylite_select_join_cache_store_scan(
    mylite_db *database,
    struct mylite_table_select_join_condition_cache *cache,
    const struct mylite_table_select_join_scan_state *scan,
    struct mylite_select_table_range range,
    bool matches
);
int mylite_select_join_cache_store_row(
    mylite_db *database,
    struct mylite_table_select_join_condition_cache *cache,
    const struct mylite_table_select_row *row,
    struct mylite_select_table_range range,
    bool matches
);
void mylite_select_join_condition_cache_deinit(
    struct mylite_table_select_join_condition_cache *cache
);

#endif
