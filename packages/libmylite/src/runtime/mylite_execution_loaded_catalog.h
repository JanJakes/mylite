#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_LOADED_CATALOG_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_LOADED_CATALOG_H

#include "mylite_catalog.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_db;

struct loaded_index_part {
    struct mylite_catalog_index_column_descriptor index_column;
    struct mylite_catalog_column_descriptor column;
    size_t column_index;
};

struct loaded_index_info {
    struct mylite_catalog_index_descriptor index;
    struct loaded_index_part *parts;
    size_t part_count;
};

struct loaded_foreign_key_part {
    struct mylite_catalog_foreign_key_column_descriptor foreign_key_column;
    struct mylite_catalog_column_descriptor child_column;
    struct mylite_catalog_column_descriptor parent_column;
    size_t child_column_index;
    size_t parent_column_index;
};

struct loaded_foreign_key_info {
    struct mylite_catalog_foreign_key_descriptor foreign_key;
    struct mylite_catalog_table_descriptor child_table;
    struct mylite_catalog_table_descriptor parent_table;
    struct mylite_catalog_index_descriptor parent_index;
    struct loaded_foreign_key_part *parts;
    size_t part_count;
};

struct loaded_check_constraint_info {
    struct mylite_catalog_check_constraint_descriptor check_constraint;
};

struct loaded_index_info_span {
    const struct loaded_index_info *indexes;
    size_t count;
};

struct primary_key_info {
    bool has_primary_key;
    struct mylite_catalog_index_descriptor index;
    struct loaded_index_part *parts;
    size_t part_count;
};

int mylite_execution_load_table_columns(
    struct mylite_db *database,
    int64_t table_id,
    struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count
);
struct primary_key_info mylite_execution_primary_key_info_init(void);
void mylite_execution_primary_key_info_deinit(struct primary_key_info *info);
int mylite_execution_load_primary_key_info(
    struct mylite_db *database,
    int64_t table_id,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct primary_key_info *out_info
);
int mylite_execution_load_table_index_infos(
    struct mylite_db *database,
    int64_t table_id,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct loaded_index_info **out_indexes,
    size_t *out_index_count
);
void mylite_execution_loaded_index_infos_deinit(
    struct loaded_index_info **indexes,
    size_t *index_count
);
void mylite_execution_loaded_index_info_deinit(struct loaded_index_info *index);
int mylite_execution_load_table_foreign_key_infos(
    struct mylite_db *database,
    int64_t table_id,
    const struct mylite_catalog_column_descriptor *child_columns,
    size_t child_column_count,
    struct loaded_foreign_key_info **out_foreign_keys,
    size_t *out_foreign_key_count
);
int mylite_execution_load_parent_foreign_key_infos(
    struct mylite_db *database,
    int64_t parent_table_id,
    struct loaded_foreign_key_info **out_foreign_keys,
    size_t *out_foreign_key_count
);
int mylite_execution_load_foreign_key_info(
    struct mylite_db *database,
    const struct mylite_catalog_foreign_key_descriptor *foreign_key,
    const struct mylite_catalog_column_descriptor *provided_child_columns,
    size_t provided_child_column_count,
    struct loaded_foreign_key_info *out_info
);
void mylite_execution_loaded_foreign_key_infos_deinit(
    struct loaded_foreign_key_info **foreign_keys,
    size_t *foreign_key_count
);
void mylite_execution_loaded_foreign_key_info_deinit(struct loaded_foreign_key_info *foreign_key);
int mylite_execution_load_table_check_constraint_infos(
    struct mylite_db *database,
    int64_t table_id,
    struct loaded_check_constraint_info **out_check_constraints,
    size_t *out_check_constraint_count
);
void mylite_execution_loaded_check_constraint_infos_deinit(
    struct loaded_check_constraint_info **check_constraints,
    size_t *check_constraint_count
);
const char *mylite_execution_column_key_text(
    struct loaded_index_info_span indexes,
    const struct primary_key_info *primary_key,
    const struct mylite_catalog_column_descriptor *column
);
bool mylite_execution_primary_key_info_contains_column_id(
    const struct primary_key_info *primary_key,
    int64_t column_id
);
bool mylite_execution_column_has_unique_secondary_index(
    struct loaded_index_info_span indexes,
    int64_t column_id
);
bool mylite_execution_column_has_nonunique_secondary_index(
    struct loaded_index_info_span indexes,
    int64_t column_id
);
int mylite_execution_reject_primary_key_table_alter(
    struct mylite_db *database,
    int64_t table_id,
    const char *message
);
int mylite_execution_reject_secondary_index_table_alter(
    struct mylite_db *database,
    int64_t table_id,
    const char *message
);
int mylite_execution_reject_check_constraint_table_alter(
    struct mylite_db *database,
    int64_t table_id,
    const char *message
);
int mylite_execution_find_column_index_by_id(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    size_t *out_index,
    int64_t column_id
);

#endif
