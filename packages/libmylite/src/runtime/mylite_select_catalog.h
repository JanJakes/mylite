#ifndef MYLITE_RUNTIME_MYLITE_SELECT_CATALOG_H
#define MYLITE_RUNTIME_MYLITE_SELECT_CATALOG_H

#include <mylite/mylite.h>

#include "mylite_select_types.h"

int mylite_select_load_table_columns(mylite_db *database, struct mylite_select_table *table);

#endif
