#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_PLAN_TYPES_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_PLAN_TYPES_H

#include "mylite_catalog.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct table_name_resolution {
    struct mylite_catalog_schema_descriptor schema;
    char table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
};

enum column_reference_diagnostic_context {
    COLUMN_REFERENCE_FIELD = 0,
    COLUMN_REFERENCE_WHERE = 1,
    COLUMN_REFERENCE_ORDER = 2,
    COLUMN_REFERENCE_GROUP = 3,
    COLUMN_REFERENCE_HAVING = 4,
    COLUMN_REFERENCE_ON = 5,
};

struct planned_drop_table_target {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    bool missing;
    bool is_temporary;
};

struct planned_drop_table {
    struct planned_drop_table_target *targets;
    size_t target_count;
    size_t missing_count;
    size_t existing_count;
    size_t temporary_existing_count;
    size_t persistent_existing_count;
    bool temporary_only;
};

struct load_data_missing_warning_request {
    size_t row_number;
    size_t warning_count;
};

struct planned_select_limit {
    bool has_limit;
    int64_t row_count;
    bool has_offset;
    int64_t offset;
};

#endif
