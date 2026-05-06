#ifndef MYLITE_RUNTIME_MYLITE_SELECT_JOIN_OUTER_MATERIALIZE_H
#define MYLITE_RUNTIME_MYLITE_SELECT_JOIN_OUTER_MATERIALIZE_H

#include <mylite/mylite.h>

struct mylite_select_eval_callbacks;

int mylite_select_materialize_outer_joined_table_result(
    mylite_stmt *stmt,
    const struct mylite_select_eval_callbacks *callbacks
);

#endif
