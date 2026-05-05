#ifndef MYLITE_RUNTIME_MYLITE_SELECT_GROUP_VALIDATE_H
#define MYLITE_RUNTIME_MYLITE_SELECT_GROUP_VALIDATE_H

#include <mylite/mylite.h>

#include "mylite_select_types.h"

#include <stdbool.h>
#include <stddef.h>

int mylite_select_validate_grouping(mylite_db *database, const struct mylite_select_plan *plan);
bool mylite_select_output_contains_aggregate(const struct mylite_select_plan *plan,
                                             size_t output_index);

#endif
