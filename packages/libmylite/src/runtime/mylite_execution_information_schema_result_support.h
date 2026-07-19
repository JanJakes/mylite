#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_RESULT_SUPPORT_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_RESULT_SUPPORT_H

#include "mylite_result.h"

#include <stddef.h>
#include <stdint.h>

struct mylite_catalog_column_descriptor;
struct mylite_db;

struct mylite_result_column_descriptor mylite_execution_information_schema_unknown_result_column_descriptor(
    const char *label
);
int mylite_execution_information_schema_populate_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor
);
uint64_t mylite_execution_information_schema_character_set_max_bytes_per_character(
    const char *character_set_name
);
uint64_t mylite_execution_information_schema_result_collation_max_bytes_per_character(
    const char *collation_name
);
uint64_t mylite_execution_information_schema_result_display_length_cap(uint64_t display_length);
uint32_t mylite_execution_information_schema_result_collation_id(const char *collation_name);

#endif
