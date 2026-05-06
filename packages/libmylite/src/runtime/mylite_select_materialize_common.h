#ifndef MYLITE_RUNTIME_MYLITE_SELECT_MATERIALIZE_COMMON_H
#define MYLITE_RUNTIME_MYLITE_SELECT_MATERIALIZE_COMMON_H

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stddef.h>

struct mylite_select_eval_callbacks;
struct mylite_table_select_group;
struct mylite_table_select_row;

int mylite_select_materialize_append_finalized_groups(
    mylite_stmt *stmt,
    struct mylite_table_select_group *groups,
    size_t group_count,
    const struct mylite_select_eval_callbacks *callbacks
);
int mylite_select_materialize_check_distinct_duplicate(
    mylite_stmt *stmt,
    struct mylite_table_select_row *row,
    bool *out_duplicate,
    const struct mylite_select_eval_callbacks *callbacks
);

#endif
