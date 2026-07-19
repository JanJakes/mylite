#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_PREDICATE_SUPPORT_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_PREDICATE_SUPPORT_H

#include <stddef.h>

struct mylite_catalog_table_descriptor;
struct mylite_db;

int mylite_execution_information_schema_auto_increment_predicate_value(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    char *buffer,
    size_t buffer_size,
    const char **out_value
);

#endif
