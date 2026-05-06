#ifndef MYLITE_RUNTIME_MYLITE_CATALOG_SCHEMA_H
#define MYLITE_RUNTIME_MYLITE_CATALOG_SCHEMA_H

#include <mylite/mylite.h>

int mylite_catalog_seed_system_schema(
    mylite_db *database,
    const char *name,
    const char *character_set,
    const char *collation
);

#endif
