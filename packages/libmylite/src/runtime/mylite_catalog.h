#ifndef MYLITE_RUNTIME_MYLITE_CATALOG_H
#define MYLITE_RUNTIME_MYLITE_CATALOG_H

#include <mylite/mylite.h>

#include "mylite_schema_types.h"

#include <stdbool.h>
#include <stdint.h>

enum mylite_catalog_table_row_delete_flags {
    MYLITE_CATALOG_DELETE_TABLE_INDEXES = 1U << 0,
    MYLITE_CATALOG_DELETE_TABLE_COLUMNS = 1U << 1,
    MYLITE_CATALOG_DELETE_TABLE_ROW = 1U << 2,
};

struct mylite_catalog_table_metadata {
    uint64_t auto_increment;
    bool has_auto_increment;
};

struct mylite_catalog_column_row {
    const char *name;
    const char *default_text;
    const char *is_nullable;
    const char *data_type;
    const char *column_type;
    const char *collation_name;
    uint64_t character_maximum_length;
    bool has_character_maximum_length;
    uint64_t numeric_precision;
    uint64_t numeric_scale;
    uint64_t datetime_precision;
    bool has_numeric_precision;
    bool has_numeric_scale;
    bool has_datetime_precision;
    const char *extra;
};

struct mylite_catalog_unique_index_part_row {
    const char *index_name;
    const char *column_name;
    uint64_t prefix_length;
    bool has_prefix_length;
};

typedef int (*mylite_catalog_column_callback)(
    void *context,
    const struct mylite_catalog_column_row *row
);
typedef int (*mylite_catalog_unique_index_part_callback)(
    void *context,
    const struct mylite_catalog_unique_index_part_row *row
);

int mylite_catalog_initialize(mylite_db *database);
int mylite_catalog_update_auto_increment(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    uint64_t next_auto_increment
);
int mylite_catalog_delete_table_rows(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    unsigned int flags
);
int mylite_catalog_delete_temporary_table_rows(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    unsigned int flags
);
int mylite_catalog_selected_schema_default(
    mylite_db *database,
    struct mylite_schema_default *out_default
);
int mylite_catalog_schema_exists(
    mylite_db *database,
    const char *schema_name,
    struct mylite_schema_presence *out_presence
);
int mylite_catalog_table_exists(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    bool *out_exists
);
int mylite_catalog_persistent_table_exists(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    bool *out_exists
);
int mylite_catalog_temporary_table_exists(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    bool *out_exists
);
int mylite_catalog_load_table_metadata(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    struct mylite_catalog_table_metadata *out_metadata
);
int mylite_catalog_load_table_columns(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    mylite_catalog_column_callback callback,
    void *context
);
int mylite_catalog_load_unique_index_parts(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    mylite_catalog_unique_index_part_callback callback,
    void *context
);
int mylite_catalog_schema_default_by_name(
    mylite_db *database,
    const char *schema_name,
    struct mylite_schema_default *out_default
);
int mylite_catalog_insert_schema(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_schema_options *options
);
int mylite_catalog_update_schema(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_schema_options *options
);
int mylite_catalog_delete_schema(mylite_db *database, const char *schema_name);
const char *mylite_catalog_table_catalog_name(bool temporary);
const char *mylite_catalog_column_catalog_name(bool temporary);
const char *mylite_catalog_index_catalog_name(bool temporary);
char *mylite_catalog_physical_table_name(const char *schema_name, const char *table_name);

#endif
