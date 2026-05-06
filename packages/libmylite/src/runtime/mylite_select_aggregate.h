#ifndef MYLITE_RUNTIME_MYLITE_SELECT_AGGREGATE_H
#define MYLITE_RUNTIME_MYLITE_SELECT_AGGREGATE_H

#include <mylite/mylite.h>

#include "mylite_select_aggregate_types.h"
#include "mylite_select_eval.h"
#include "mylite_select_types.h"

int mylite_select_update_aggregate_state(
    mylite_stmt *stmt,
    struct mylite_select_aggregate_state *state,
    const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks
);
int mylite_select_finalize_aggregate_state(
    mylite_stmt *stmt,
    const struct mylite_select_aggregate_state *state,
    const struct mylite_select_aggregate_binding *binding,
    struct mylite_expression_value *out_value
);
void mylite_select_aggregate_state_deinit(struct mylite_select_aggregate_state *state);

int mylite_select_aggregate_value_to_double(
    struct mylite_expression_warnings *warnings,
    const struct mylite_expression_value *value,
    struct mylite_aggregate_numeric_value *out_value
);
int mylite_select_aggregate_format_double(double value, struct mylite_expression_value *out_value);

#endif
