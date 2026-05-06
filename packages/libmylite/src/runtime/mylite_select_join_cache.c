#include "mylite_select_join_cache.h"

#include "mylite_diagnostics.h"
#include "mylite_select.h"
#include "mylite_select_resolve.h"

#include <stdlib.h>

static bool select_join_predicate_cache_range(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_select_join_predicate *predicate,
    struct mylite_select_table_range *out_range
);

static bool select_expression_referenced_table_range(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_select_table_range scope,
    bool *out_seen,
    struct mylite_select_table_range *out_range
);

static bool select_column_index_table_range(
    const struct mylite_select_plan *plan,
    size_t column_index,
    struct mylite_select_table_range *out_range
);

static bool select_join_stage_cache_range_merge(
    struct mylite_select_table_range candidate,
    struct mylite_select_table_range *out_range,
    bool *out_seen
);

static bool table_select_join_cache_row_indexes_match(
    const struct mylite_table_select_join_condition_cache_entry *entry,
    const struct mylite_table_select_join_scan_state *scan
);

static bool table_select_join_cache_row_source_indexes_match(
    const struct mylite_table_select_join_condition_cache_entry *entry,
    const struct mylite_table_select_row *row
);

bool mylite_select_join_cache_stage_range(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    size_t available_table_count,
    struct mylite_select_table_range *out_range
) {
    struct mylite_select_table_range range = {0};
    bool seen = false;

    for (size_t index = 0U; index < plan->using_column_count; ++index) {
        const struct mylite_select_join_using_column *column = &plan->using_columns[index];

        if (column->first_table + column->table_count != available_table_count) {
            continue;
        }
        if (!select_join_stage_cache_range_merge(
                (struct mylite_select_table_range){
                    .first_table = column->first_table,
                    .table_count = column->table_count,
                },
                &range,
                &seen
            )) {
            return false;
        }
    }

    for (size_t index = 0U; index < plan->join_predicate_count; ++index) {
        const struct mylite_select_join_predicate *predicate = &plan->join_predicates[index];
        struct mylite_select_table_range predicate_range = {0};

        if (predicate->first_table + predicate->table_count != available_table_count) {
            continue;
        }
        if (!select_join_predicate_cache_range(database, plan, predicate, &predicate_range) ||
            !select_join_stage_cache_range_merge(predicate_range, &range, &seen)) {
            return false;
        }
    }
    *out_range = range;

    if (!seen) {
        return false;
    }
    if (range.table_count == 0U) {
        return true;
    }
    if (range.first_table > 0U) {
        return true;
    }
    return false;
}

int mylite_select_join_cache_lookup_scan(
    const struct mylite_table_select_join_condition_cache *cache,
    const struct mylite_table_select_join_scan_state *scan,
    struct mylite_select_table_range range,
    struct mylite_table_select_join_condition_cache_lookup *out_lookup
) {
    *out_lookup = (struct mylite_table_select_join_condition_cache_lookup){
        .found = false,
        .matches = false,
    };
    if (range.first_table > scan->table_count ||
        range.table_count > scan->table_count - range.first_table) {
        return MYLITE_UNSUPPORTED;
    }

    for (size_t index = 0U; index < cache->entry_count; ++index) {
        const struct mylite_table_select_join_condition_cache_entry *entry = &cache->entries[index];

        if (entry->first_table == range.first_table && entry->table_count == range.table_count &&
            table_select_join_cache_row_indexes_match(entry, scan)) {
            out_lookup->found = true;
            out_lookup->matches = entry->matches;
            return MYLITE_OK;
        }
    }
    return MYLITE_OK;
}

int mylite_select_join_cache_lookup_row(
    const struct mylite_table_select_join_condition_cache *cache,
    const struct mylite_table_select_row *row,
    struct mylite_select_table_range range,
    struct mylite_table_select_join_condition_cache_lookup *out_lookup
) {
    *out_lookup = (struct mylite_table_select_join_condition_cache_lookup){
        .found = false,
        .matches = false,
    };
    if (row == NULL || range.first_table > row->source_row_index_count ||
        range.table_count > row->source_row_index_count - range.first_table) {
        return MYLITE_UNSUPPORTED;
    }

    for (size_t index = 0U; index < cache->entry_count; ++index) {
        const struct mylite_table_select_join_condition_cache_entry *entry = &cache->entries[index];

        if (entry->first_table == range.first_table && entry->table_count == range.table_count &&
            table_select_join_cache_row_source_indexes_match(entry, row)) {
            out_lookup->found = true;
            out_lookup->matches = entry->matches;
            return MYLITE_OK;
        }
    }
    return MYLITE_OK;
}

int mylite_select_join_cache_store_scan(
    mylite_db *database,
    struct mylite_table_select_join_condition_cache *cache,
    const struct mylite_table_select_join_scan_state *scan,
    struct mylite_select_table_range range,
    bool matches
) {
    struct mylite_table_select_join_condition_cache_entry entry = {
        .first_table = range.first_table,
        .table_count = range.table_count,
        .matches = matches,
    };
    struct mylite_table_select_join_condition_cache_entry *entries = NULL;

    if (range.first_table > scan->table_count ||
        range.table_count > scan->table_count - range.first_table) {
        return MYLITE_UNSUPPORTED;
    }
    if (range.table_count != 0U) {
        entry.row_indexes = calloc(range.table_count, sizeof(*entry.row_indexes));
    }
    if (entry.row_indexes == NULL && range.table_count != 0U) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < range.table_count; ++index) {
        entry.row_indexes[index] = scan->frames[range.first_table + index].row_index;
    }

    entries = realloc(cache->entries, (cache->entry_count + 1U) * sizeof(*cache->entries));
    if (entries == NULL) {
        free(entry.row_indexes);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    cache->entries = entries;
    cache->entries[cache->entry_count++] = entry;
    return MYLITE_OK;
}

int mylite_select_join_cache_store_row(
    mylite_db *database,
    struct mylite_table_select_join_condition_cache *cache,
    const struct mylite_table_select_row *row,
    struct mylite_select_table_range range,
    bool matches
) {
    struct mylite_table_select_join_condition_cache_entry entry = {
        .first_table = range.first_table,
        .table_count = range.table_count,
        .matches = matches,
    };
    struct mylite_table_select_join_condition_cache_entry *entries = NULL;

    if (row == NULL || range.first_table > row->source_row_index_count ||
        range.table_count > row->source_row_index_count - range.first_table) {
        return MYLITE_UNSUPPORTED;
    }
    if (range.table_count != 0U) {
        entry.row_indexes = calloc(range.table_count, sizeof(*entry.row_indexes));
    }
    if (entry.row_indexes == NULL && range.table_count != 0U) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < range.table_count; ++index) {
        entry.row_indexes[index] = row->source_row_indexes[range.first_table + index];
    }

    entries = realloc(cache->entries, (cache->entry_count + 1U) * sizeof(*cache->entries));
    if (entries == NULL) {
        free(entry.row_indexes);
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    cache->entries = entries;
    cache->entries[cache->entry_count++] = entry;
    return MYLITE_OK;
}

void mylite_select_join_condition_cache_deinit(
    struct mylite_table_select_join_condition_cache *cache
) {
    if (cache == NULL) {
        return;
    }
    for (size_t index = 0U; index < cache->entry_count; ++index) {
        free(cache->entries[index].row_indexes);
    }
    free(cache->entries);
    *cache = (struct mylite_table_select_join_condition_cache){0};
}

static bool select_join_predicate_cache_range(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_select_join_predicate *predicate,
    struct mylite_select_table_range *out_range
) {
    bool seen = false;
    struct mylite_select_table_range range = {
        .first_table = predicate->first_table + predicate->table_count,
    };

    if (!select_expression_referenced_table_range(
            database,
            plan,
            predicate->expression,
            (struct mylite_select_table_range){
                .first_table = predicate->first_table,
                .table_count = predicate->table_count,
            },
            &seen,
            &range
        )) {
        *out_range = (struct mylite_select_table_range){
            .first_table = predicate->first_table,
            .table_count = predicate->table_count,
        };
        return true;
    }
    *out_range = range;
    return true;
}

// NOLINTNEXTLINE(misc-no-recursion)
static bool select_expression_referenced_table_range(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_select_table_range scope,
    bool *out_seen,
    struct mylite_select_table_range *out_range
) {
    if (expression == NULL) {
        return true;
    }

    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        size_t column_index = mylite_select_plan_column_count(plan);
        struct mylite_select_table_range column_range = {0};

        if (mylite_select_resolve_plan_column_reference_in_scope(
                database,
                plan,
                expression,
                "on clause",
                scope.first_table,
                scope.table_count,
                &column_index
            ) != MYLITE_OK ||
            !select_column_index_table_range(plan, column_index, &column_range)) {
            return false;
        }
        return select_join_stage_cache_range_merge(column_range, out_range, out_seen);
    }

    for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
         child = child->next_sibling) {
        if (!select_expression_referenced_table_range(
                database,
                plan,
                child,
                scope,
                out_seen,
                out_range
            )) {
            return false;
        }
    }
    return true;
}

static bool select_column_index_table_range(
    const struct mylite_select_plan *plan,
    size_t column_index,
    struct mylite_select_table_range *out_range
) {
    for (size_t table_index = 0U; table_index < mylite_select_plan_table_count(plan);
         ++table_index) {
        const struct mylite_select_table *table = mylite_select_plan_table_const(plan, table_index);

        if (table != NULL && column_index >= table->first_column_index &&
            column_index < table->first_column_index + table->column_count) {
            *out_range = (struct mylite_select_table_range){
                .first_table = table_index,
                .table_count = 1U,
            };
            return true;
        }
    }
    return false;
}

static bool select_join_stage_cache_range_merge(
    struct mylite_select_table_range candidate,
    struct mylite_select_table_range *out_range,
    bool *out_seen
) {
    size_t range_start = out_range->first_table;
    size_t range_end = out_range->first_table + out_range->table_count;
    size_t candidate_end = candidate.first_table + candidate.table_count;

    if (!*out_seen) {
        *out_range = candidate;
        *out_seen = true;
        return true;
    }
    if (candidate.table_count == 0U) {
        return true;
    }
    if (out_range->table_count == 0U) {
        *out_range = candidate;
        return true;
    }
    if (candidate.first_table < range_start) {
        range_start = candidate.first_table;
    }
    if (candidate_end > range_end) {
        range_end = candidate_end;
    }
    out_range->first_table = range_start;
    out_range->table_count = range_end - range_start;
    return true;
}

static bool table_select_join_cache_row_indexes_match(
    const struct mylite_table_select_join_condition_cache_entry *entry,
    const struct mylite_table_select_join_scan_state *scan
) {
    for (size_t index = 0U; index < entry->table_count; ++index) {
        if (entry->row_indexes[index] != scan->frames[entry->first_table + index].row_index) {
            return false;
        }
    }
    return true;
}

static bool table_select_join_cache_row_source_indexes_match(
    const struct mylite_table_select_join_condition_cache_entry *entry,
    const struct mylite_table_select_row *row
) {
    for (size_t index = 0U; index < entry->table_count; ++index) {
        if (entry->row_indexes[index] != row->source_row_indexes[entry->first_table + index]) {
            return false;
        }
    }
    return true;
}
