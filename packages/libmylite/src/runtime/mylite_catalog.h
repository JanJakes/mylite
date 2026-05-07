#ifndef MYLITE_RUNTIME_MYLITE_CATALOG_H
#define MYLITE_RUNTIME_MYLITE_CATALOG_H

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    MYLITE_CATALOG_SCHEMA_VERSION = 1,
    MYLITE_CATALOG_MINIMUM_READER_SCHEMA_VERSION = 1,
    MYLITE_CATALOG_IDENTIFIER_CAPACITY = 64,
    MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY = 128,
    MYLITE_CATALOG_TYPE_NAME_CAPACITY = 64,
};

enum mylite_catalog_table_kind {
    MYLITE_CATALOG_TABLE_KIND_INVALID = 0,
    MYLITE_CATALOG_TABLE_KIND_BASE = 1,
};

struct mylite_db;

struct mylite_catalog {
    bool initialized;
    uint32_t schema_version;
    uint64_t generation;
    uint64_t cached_generation;
    bool descriptor_cache_is_valid;
};

struct mylite_catalog_schema_descriptor {
    int64_t schema_id;
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    uint64_t descriptor_version;
    uint64_t created_catalog_generation;
    uint64_t updated_catalog_generation;
};

struct mylite_catalog_table_descriptor {
    int64_t table_id;
    int64_t schema_id;
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    enum mylite_catalog_table_kind kind;
    char physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    uint64_t descriptor_version;
    uint64_t created_catalog_generation;
    uint64_t updated_catalog_generation;
};

struct mylite_catalog_column_descriptor {
    int64_t column_id;
    int64_t table_id;
    int64_t ordinal_position;
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char logical_type[MYLITE_CATALOG_TYPE_NAME_CAPACITY];
    char physical_type[MYLITE_CATALOG_TYPE_NAME_CAPACITY];
    bool is_nullable;
    uint64_t descriptor_version;
    uint64_t created_catalog_generation;
    uint64_t updated_catalog_generation;
};

struct mylite_catalog_mutation {
    bool active;
    uint64_t next_generation;
};

typedef int (*mylite_catalog_table_callback)(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);

void mylite_catalog_init(struct mylite_catalog *catalog);
void mylite_catalog_deinit(struct mylite_catalog *catalog);

int mylite_catalog_initialize_file_backed(struct mylite_db *database);
void mylite_catalog_invalidate_descriptor_cache(struct mylite_db *database);

void mylite_catalog_mutation_init(struct mylite_catalog_mutation *mutation);
void mylite_catalog_mutation_deinit(struct mylite_catalog_mutation *mutation);
int mylite_catalog_begin_mutation(
    struct mylite_db *database,
    struct mylite_catalog_mutation *mutation
);
int mylite_catalog_commit_mutation(
    struct mylite_db *database,
    struct mylite_catalog_mutation *mutation
);
void mylite_catalog_rollback_mutation(
    struct mylite_db *database,
    struct mylite_catalog_mutation *mutation
);
uint64_t mylite_catalog_mutation_generation(const struct mylite_catalog_mutation *mutation);

int mylite_catalog_allocate_table_id_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t *out_table_id
);
int mylite_catalog_insert_table_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t schema_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_table_kind kind,
    struct mylite_catalog_table_descriptor *out_table
);
int mylite_catalog_insert_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t ordinal_position,
    const char *name,
    const char *logical_type,
    const char *physical_type,
    bool is_nullable,
    struct mylite_catalog_column_descriptor *out_column
);
int mylite_catalog_delete_table_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id
);
int mylite_catalog_update_table_identity_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table
);
int mylite_catalog_for_each_table_in_schema(
    struct mylite_db *database,
    int64_t schema_id,
    mylite_catalog_table_callback callback,
    void *user_data
);

int mylite_catalog_create_schema(
    struct mylite_db *database,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema
);
int mylite_catalog_read_schema_by_name(
    struct mylite_db *database,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema
);
int mylite_catalog_delete_schema(struct mylite_db *database, int64_t schema_id);

int mylite_catalog_create_table(
    struct mylite_db *database,
    int64_t schema_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_table_kind kind,
    struct mylite_catalog_table_descriptor *out_table
);
int mylite_catalog_read_table_by_name(
    struct mylite_db *database,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table
);
int mylite_catalog_update_table_name(
    struct mylite_db *database,
    int64_t table_id,
    const char *name
);
int mylite_catalog_delete_table(struct mylite_db *database, int64_t table_id);

int mylite_catalog_create_column(
    struct mylite_db *database,
    int64_t table_id,
    int64_t ordinal_position,
    const char *name,
    const char *logical_type,
    const char *physical_type,
    bool is_nullable,
    struct mylite_catalog_column_descriptor *out_column
);
int mylite_catalog_read_column_by_name(
    struct mylite_db *database,
    int64_t table_id,
    const char *name,
    struct mylite_catalog_column_descriptor *out_column
);
int mylite_catalog_delete_column(struct mylite_db *database, int64_t column_id);

bool mylite_catalog_name_is_reserved(const char *name);

#endif
