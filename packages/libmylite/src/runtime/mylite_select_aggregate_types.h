#ifndef MYLITE_RUNTIME_MYLITE_SELECT_AGGREGATE_TYPES_H
#define MYLITE_RUNTIME_MYLITE_SELECT_AGGREGATE_TYPES_H

#include "mylite_expression.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_count_distinct_tuple {
    struct mylite_expression_value *values;
    size_t value_count;
};

struct mylite_group_concat_item {
    struct mylite_expression_value *arguments;
    struct mylite_expression_value *order_values;
    char *text;
    size_t argument_count;
    size_t order_value_count;
    size_t text_length;
};

struct mylite_select_aggregate_state {
    struct mylite_expression_value value;
    struct mylite_count_distinct_tuple *distinct_tuples;
    struct mylite_group_concat_item *group_concat_items;
    uint64_t count;
    uint64_t non_null_count;
    double sum;
    size_t distinct_tuple_count;
    size_t group_concat_item_count;
    bool integral_sum;
    bool unsigned_sum;
    bool has_value;
};

struct mylite_aggregate_numeric_value {
    double value;
    bool integral;
    bool unsigned_value;
};

#endif
