#ifndef MYLITE_RUNTIME_MYLITE_METADATA_TYPES_H
#define MYLITE_RUNTIME_MYLITE_METADATA_TYPES_H

#include "mylite_field_descriptor.h"

#include <stddef.h>

struct mylite_result_column_metadata {
    char *name;
    char *schema_name;
    char *table_name;
    char *origin_schema_name;
    char *origin_table_name;
    char *origin_column_name;
    struct mylite_field_descriptor descriptor;
};

struct mylite_result_metadata {
    struct mylite_result_column_metadata *columns;
    size_t column_count;
};

#endif
