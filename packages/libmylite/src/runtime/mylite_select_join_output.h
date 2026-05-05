#ifndef MYLITE_RUNTIME_MYLITE_SELECT_JOIN_OUTPUT_H
#define MYLITE_RUNTIME_MYLITE_SELECT_JOIN_OUTPUT_H

#include <mylite/mylite.h>

struct mylite_select_eval_callbacks;
struct mylite_table_select_join_materialize_state;
struct mylite_table_select_row;

int mylite_select_join_materialize_row(mylite_stmt *stmt,
                                       struct mylite_table_select_join_materialize_state *state,
                                       const struct mylite_table_select_row *row,
                                       const struct mylite_select_eval_callbacks *callbacks);

#endif
