#ifndef MYLITE_RUNTIME_MYLITE_TEMPORARY_CATALOG_H
#define MYLITE_RUNTIME_MYLITE_TEMPORARY_CATALOG_H

#include "mylite_catalog.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_temporary_catalog_table {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
    struct mylite_catalog_index_descriptor *indexes;
    size_t index_count;
    struct mylite_catalog_index_column_descriptor *index_columns;
    size_t index_column_count;
};

struct mylite_temporary_catalog {
    bool initialized;
    int64_t next_table_id;
    int64_t next_column_id;
    int64_t next_index_id;
    int64_t next_index_column_id;
    uint64_t next_physical_table_id;
    uint64_t next_physical_index_id;
    struct mylite_temporary_catalog_table *tables;
    size_t table_count;
    size_t table_capacity;
};

void mylite_temporary_catalog_init(struct mylite_temporary_catalog *catalog);
void mylite_temporary_catalog_deinit(struct mylite_temporary_catalog *catalog);

int mylite_temporary_catalog_allocate_table_identity(
    struct mylite_temporary_catalog *catalog,
    int64_t *out_table_id,
    char *physical_name,
    size_t physical_name_size
);
int mylite_temporary_catalog_allocate_column_id(
    struct mylite_temporary_catalog *catalog,
    int64_t *out_column_id
);
int mylite_temporary_catalog_allocate_index_identity(
    struct mylite_temporary_catalog *catalog,
    int64_t *out_index_id,
    char *physical_name,
    size_t physical_name_size
);
int mylite_temporary_catalog_allocate_index_column_id(
    struct mylite_temporary_catalog *catalog,
    int64_t *out_index_column_id
);

int mylite_temporary_catalog_append_table(
    struct mylite_temporary_catalog *catalog,
    struct mylite_temporary_catalog_table *table
);
int mylite_temporary_catalog_remove_table_by_id(
    struct mylite_temporary_catalog *catalog,
    int64_t table_id
);
int mylite_temporary_catalog_update_table_auto_increment_next(
    struct mylite_temporary_catalog *catalog,
    int64_t table_id, // NOLINT(bugprone-easily-swappable-parameters): mirror durable catalog API.
    int64_t auto_increment_next
);
int mylite_temporary_catalog_update_table_comment(
    struct mylite_temporary_catalog *catalog,
    int64_t table_id,
    const char *comment
);
int mylite_temporary_catalog_append_index(
    struct mylite_temporary_catalog *catalog,
    int64_t table_id,
    const struct mylite_catalog_index_descriptor *index,
    const struct mylite_catalog_index_column_descriptor *index_columns,
    size_t index_column_count
);
int mylite_temporary_catalog_remove_index_by_id(
    struct mylite_temporary_catalog *catalog,
    int64_t table_id,
    int64_t index_id
);

int mylite_temporary_catalog_try_read_table_by_name(
    const struct mylite_temporary_catalog *catalog,
    const char *schema_name,
    const char *table_name,
    struct mylite_catalog_table_descriptor *out_table,
    bool *out_found
);
int mylite_temporary_catalog_for_each_column_in_table(
    const struct mylite_temporary_catalog *catalog,
    int64_t table_id,
    mylite_catalog_column_callback callback,
    void *user_data
);
int mylite_temporary_catalog_for_each_index_in_table(
    const struct mylite_temporary_catalog *catalog,
    int64_t table_id,
    mylite_catalog_index_callback callback,
    void *user_data
);
int mylite_temporary_catalog_for_each_index_column_in_index(
    const struct mylite_temporary_catalog *catalog,
    int64_t index_id,
    mylite_catalog_index_column_callback callback,
    void *user_data
);
int mylite_temporary_catalog_try_read_primary_index_by_table_id(
    const struct mylite_temporary_catalog *catalog,
    int64_t table_id,
    struct mylite_catalog_index_descriptor *out_index,
    bool *out_found
);

void mylite_temporary_catalog_table_deinit(struct mylite_temporary_catalog_table *table);

#endif
