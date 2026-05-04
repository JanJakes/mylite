#ifndef MYLITE_RUNTIME_MYLITE_CATALOG_H
#define MYLITE_RUNTIME_MYLITE_CATALOG_H

#include <mylite/mylite.h>

#include <stdint.h>

int mylite_catalog_initialize(mylite_db *database);
int mylite_catalog_update_auto_increment(mylite_db *database, const char *schema_name,
                                         const char *table_name, uint64_t next_auto_increment);

#endif
