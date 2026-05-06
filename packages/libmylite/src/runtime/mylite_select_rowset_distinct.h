#ifndef MYLITE_RUNTIME_MYLITE_SELECT_ROWSET_DISTINCT_H
#define MYLITE_RUNTIME_MYLITE_SELECT_ROWSET_DISTINCT_H

#include "mylite_select_types.h"

#include <stdbool.h>

struct mylite_field_descriptor;
struct mylite_result_metadata;

bool mylite_select_result_distinct_row_exists(
    const struct mylite_table_select_result *result,
    const struct mylite_select_plan *plan,
    const struct mylite_result_metadata *metadata,
    const struct mylite_table_select_row *row
);
bool mylite_select_output_values_equal(
    const struct mylite_select_plan *plan,
    const struct mylite_result_metadata *metadata,
    const struct mylite_table_select_row *left,
    const struct mylite_table_select_row *right
);
int mylite_select_compare_distinct_values(
    const struct mylite_expression_value *left,
    const struct mylite_expression_value *right,
    const struct mylite_field_descriptor *descriptor
);

#endif
