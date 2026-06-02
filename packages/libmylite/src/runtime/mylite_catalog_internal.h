#ifndef MYLITE_RUNTIME_MYLITE_CATALOG_INTERNAL_H
#define MYLITE_RUNTIME_MYLITE_CATALOG_INTERNAL_H

#include "mylite_catalog.h"
#include "sqlite3.h"

#include <stdint.h>

int mylite_catalog_migrate_schema_one_step(sqlite3 *sqlite, uint32_t *schema_version);

#endif
