#ifndef MYLITE_RUNTIME_MYLITE_SELECT_JOIN_RANGE_ROWSET_H
#define MYLITE_RUNTIME_MYLITE_SELECT_JOIN_RANGE_ROWSET_H

#include <mylite/mylite.h>

struct mylite_select_eval_callbacks;
struct mylite_select_table_range;
struct mylite_table_select_join_materialize_state;
struct mylite_table_select_table_rowset;

int mylite_select_join_range_rowset_materialize(
    mylite_stmt *stmt, struct mylite_table_select_join_materialize_state *state,
    const struct mylite_select_table_range *range,
    struct mylite_table_select_table_rowset *out_rowset,
    const struct mylite_select_eval_callbacks *callbacks);

#endif
