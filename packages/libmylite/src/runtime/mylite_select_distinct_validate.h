#ifndef MYLITE_RUNTIME_MYLITE_SELECT_DISTINCT_VALIDATE_H
#define MYLITE_RUNTIME_MYLITE_SELECT_DISTINCT_VALIDATE_H

#include <mylite/mylite.h>

#include "mylite_select_types.h"

int mylite_select_validate_distinct_order(mylite_db *database,
                                          const struct mylite_select_plan *plan);

#endif
