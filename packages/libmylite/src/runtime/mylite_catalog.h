#ifndef MYLITE_RUNTIME_MYLITE_CATALOG_H
#define MYLITE_RUNTIME_MYLITE_CATALOG_H

#include "mylite_runtime.h"

#include <stdbool.h>
#include <stdint.h>

int mylite_catalog_initialize(mylite_db *database);
int mylite_catalog_update_auto_increment(mylite_db *database, const char *schema_name,
                                         const char *table_name, uint64_t next_auto_increment);
int mylite_catalog_selected_schema_default(mylite_db *database,
                                           struct mylite_schema_default *out_default);
int mylite_catalog_schema_exists(mylite_db *database, const char *schema_name,
                                 struct mylite_schema_presence *out_presence);
int mylite_catalog_table_exists(mylite_db *database, const char *schema_name,
                                const char *table_name, bool *out_exists);
int mylite_catalog_schema_default_by_name(mylite_db *database, const char *schema_name,
                                          struct mylite_schema_default *out_default);
int mylite_catalog_insert_schema(mylite_db *database, const char *schema_name,
                                 const struct mylite_schema_options *options);
int mylite_catalog_update_schema(mylite_db *database, const char *schema_name,
                                 const struct mylite_schema_options *options);
int mylite_catalog_delete_schema(mylite_db *database, const char *schema_name);
char *mylite_catalog_physical_table_name(const char *schema_name, const char *table_name);

#endif
