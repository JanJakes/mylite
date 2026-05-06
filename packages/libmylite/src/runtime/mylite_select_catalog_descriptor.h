#ifndef MYLITE_RUNTIME_MYLITE_SELECT_CATALOG_DESCRIPTOR_H
#define MYLITE_RUNTIME_MYLITE_SELECT_CATALOG_DESCRIPTOR_H

#include <mylite/mylite.h>

#include "mylite_field_descriptor.h"
#include "sqlite3.h"

int mylite_select_catalog_load_column_descriptor(
    mylite_db *database,
    sqlite3_stmt *select,
    struct mylite_field_descriptor *out_descriptor
);

#endif
