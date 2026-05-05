#ifndef MYLITE_RUNTIME_MYLITE_SELECT_USING_RANGE_H
#define MYLITE_RUNTIME_MYLITE_SELECT_USING_RANGE_H

#include "mylite_select_types.h"

#include <stdbool.h>
#include <stddef.h>

size_t mylite_select_count_column_parts_using_matches(const struct mylite_select_plan *plan,
                                                      const char *column_name,
                                                      struct mylite_select_table_range range,
                                                      size_t *match_index);
bool mylite_select_column_index_is_using_column_in_range(const struct mylite_select_plan *plan,
                                                         size_t column_index,
                                                         struct mylite_select_table_range range);

#endif
