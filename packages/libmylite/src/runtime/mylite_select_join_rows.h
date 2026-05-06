#ifndef MYLITE_RUNTIME_MYLITE_SELECT_JOIN_ROWS_H
#define MYLITE_RUNTIME_MYLITE_SELECT_JOIN_ROWS_H

#include <mylite/mylite.h>

#include "mylite_select_types.h"

#include <stdbool.h>
#include <stddef.h>

int mylite_select_join_rowset_append_null_extended_left(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *left_row,
    struct mylite_table_select_table_rowset *out_rowset
);
int mylite_select_join_rowset_append_null_extended_right_unmatched(
    mylite_stmt *stmt,
    const struct mylite_select_join_step *step,
    const struct mylite_table_select_table_rowset *right,
    const bool *right_matched,
    struct mylite_table_select_table_rowset *out_rowset
);
int mylite_select_join_rowset_append_empty(
    mylite_stmt *stmt,
    struct mylite_table_select_table_rowset *rowset,
    struct mylite_table_select_row **out_row
);
int mylite_select_join_row_copy_base_table_values(
    mylite_db *database,
    struct mylite_table_select_row *row,
    const struct mylite_select_table *table,
    size_t table_index,
    const struct mylite_table_select_row *source,
    size_t source_row_index
);
int mylite_select_join_row_copy_range_values(
    struct mylite_table_select_row *target,
    const struct mylite_table_select_row *source,
    struct mylite_select_table_range range,
    const struct mylite_select_plan *plan
);
int mylite_select_join_row_copy_table_values(
    struct mylite_table_select_row *row,
    const struct mylite_select_table *table,
    const struct mylite_table_select_row *source
);
void mylite_select_join_row_clear_table_values(
    struct mylite_table_select_row *row,
    const struct mylite_select_table *table
);

#endif
