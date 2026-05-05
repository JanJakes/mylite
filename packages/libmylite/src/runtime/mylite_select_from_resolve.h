#ifndef MYLITE_RUNTIME_MYLITE_SELECT_FROM_RESOLVE_H
#define MYLITE_RUNTIME_MYLITE_SELECT_FROM_RESOLVE_H

#include <mylite/mylite.h>

#include "mylite_select_types.h"

int mylite_select_from_resolve_tables(mylite_db *database, struct mylite_select_plan *plan);

#endif
