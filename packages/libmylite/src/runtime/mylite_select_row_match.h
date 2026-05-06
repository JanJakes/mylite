#ifndef MYLITE_RUNTIME_MYLITE_SELECT_ROW_MATCH_H
#define MYLITE_RUNTIME_MYLITE_SELECT_ROW_MATCH_H

#include <mylite/mylite.h>

#include "mylite_select_types.h"

#include <stdbool.h>

struct mylite_select_eval_callbacks;

int mylite_select_row_matches(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    bool *out_matches,
    const struct mylite_select_eval_callbacks *callbacks
);
int mylite_select_join_step_conditions_match(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_row *row,
    const struct mylite_select_join_step *step,
    bool *out_matches,
    const struct mylite_select_eval_callbacks *callbacks
);
int mylite_select_join_stage_conditions_match(
    mylite_stmt *stmt,
    struct mylite_table_select_join_materialize_state *state,
    const struct mylite_table_select_join_scan_state *scan,
    bool *out_matches,
    const struct mylite_select_eval_callbacks *callbacks
);

#endif
