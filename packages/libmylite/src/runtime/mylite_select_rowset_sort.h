#ifndef MYLITE_RUNTIME_MYLITE_SELECT_ROWSET_SORT_H
#define MYLITE_RUNTIME_MYLITE_SELECT_ROWSET_SORT_H

#include <mylite/mylite.h>

#include "mylite_select_types.h"

int mylite_select_result_sort_rows(mylite_db *database, struct mylite_table_select_result *result,
                                   const struct mylite_select_plan *plan);

#endif
