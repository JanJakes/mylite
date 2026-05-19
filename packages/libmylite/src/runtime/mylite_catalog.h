#ifndef MYLITE_RUNTIME_MYLITE_CATALOG_H
#define MYLITE_RUNTIME_MYLITE_CATALOG_H

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MYLITE_CATALOG_STRINGIFY_DETAIL(value) #value
#define MYLITE_CATALOG_STRINGIFY(value) MYLITE_CATALOG_STRINGIFY_DETAIL(value)
#define MYLITE_CATALOG_SCHEMA_VERSION_VALUE 24
#define MYLITE_CATALOG_MINIMUM_READER_SCHEMA_VERSION_VALUE 24
#define MYLITE_CATALOG_SCHEMA_VERSION_TEXT                                                         \
    MYLITE_CATALOG_STRINGIFY(MYLITE_CATALOG_SCHEMA_VERSION_VALUE)
#define MYLITE_CATALOG_MINIMUM_READER_SCHEMA_VERSION_TEXT                                          \
    MYLITE_CATALOG_STRINGIFY(MYLITE_CATALOG_MINIMUM_READER_SCHEMA_VERSION_VALUE)

enum {
    MYLITE_CATALOG_SCHEMA_VERSION = MYLITE_CATALOG_SCHEMA_VERSION_VALUE,
    MYLITE_CATALOG_MINIMUM_READER_SCHEMA_VERSION =
        MYLITE_CATALOG_MINIMUM_READER_SCHEMA_VERSION_VALUE,
    MYLITE_CATALOG_IDENTIFIER_CAPACITY = 64,
    MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY = 128,
    MYLITE_CATALOG_TYPE_NAME_CAPACITY = 1024,
    MYLITE_CATALOG_DEFAULT_TEXT_CAPACITY = 1024,
    MYLITE_CATALOG_UTF8MB4_MAX_BYTES_PER_CHARACTER = 4,
    MYLITE_CATALOG_TABLE_COMMENT_MAX_CHARACTERS = 2048,
    MYLITE_CATALOG_TABLE_COMMENT_CAPACITY = (MYLITE_CATALOG_TABLE_COMMENT_MAX_CHARACTERS *
                                             MYLITE_CATALOG_UTF8MB4_MAX_BYTES_PER_CHARACTER) +
                                            1,
    MYLITE_CATALOG_CHECK_CLAUSE_CAPACITY = 4096,
};

#define MYLITE_CATALOG_DEFAULT_TABLE_CHARSET "utf8mb4"
#define MYLITE_CATALOG_DEFAULT_TABLE_COLLATION "utf8mb4_0900_ai_ci"

enum mylite_catalog_table_kind {
    MYLITE_CATALOG_TABLE_KIND_INVALID = 0,
    MYLITE_CATALOG_TABLE_KIND_BASE = 1,
    MYLITE_CATALOG_TABLE_KIND_TEMPORARY = 2,
};

enum mylite_catalog_column_default_kind {
    MYLITE_CATALOG_COLUMN_DEFAULT_NONE = 0,
    MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER = 1,
    MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT = 2,
    MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL = 3,
    MYLITE_CATALOG_COLUMN_DEFAULT_TEXT = 4,
    MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION = 5,
    MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION = 6,
    MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIMESTAMP = 7,
    MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_DATE = 8,
    MYLITE_CATALOG_COLUMN_DEFAULT_CURRENT_TIME = 9,
};

enum mylite_catalog_index_kind {
    MYLITE_CATALOG_INDEX_KIND_INVALID = 0,
    MYLITE_CATALOG_INDEX_KIND_PRIMARY = 1,
    MYLITE_CATALOG_INDEX_KIND_SECONDARY = 2,
    MYLITE_CATALOG_INDEX_KIND_FULLTEXT = 3,
};

enum mylite_catalog_index_sort_direction {
    MYLITE_CATALOG_INDEX_SORT_DIRECTION_INVALID = 0,
    MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC = 1,
    MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC = 2,
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
    char default_charset[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char default_collation[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
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
    int64_t auto_increment_next;
    char default_charset[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char default_collation[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char comment[MYLITE_CATALOG_TABLE_COMMENT_CAPACITY];
    bool fulltext_doc_id_initialized;
    int64_t created_time_utc_epoch;
    int64_t updated_time_utc_epoch;
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
    bool is_visible;
    bool is_auto_increment;
    enum mylite_catalog_column_default_kind default_kind;
    int64_t default_integer;
    char default_text[MYLITE_CATALOG_DEFAULT_TEXT_CAPACITY];
    bool on_update_current_timestamp;
    char character_set_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    uint64_t descriptor_version;
    uint64_t created_catalog_generation;
    uint64_t updated_catalog_generation;
};

struct mylite_catalog_index_descriptor {
    int64_t index_id;
    int64_t table_id;
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    enum mylite_catalog_index_kind kind;
    bool is_unique;
    bool is_visible;
    char physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    uint64_t descriptor_version;
    uint64_t created_catalog_generation;
    uint64_t updated_catalog_generation;
};

struct mylite_catalog_index_column_descriptor {
    int64_t index_column_id;
    int64_t index_id;
    int64_t table_id;
    int64_t column_id;
    int64_t ordinal_position;
    bool has_prefix_length;
    int64_t prefix_length;
    enum mylite_catalog_index_sort_direction sort_direction;
    uint64_t descriptor_version;
    uint64_t created_catalog_generation;
    uint64_t updated_catalog_generation;
};

struct mylite_catalog_foreign_key_descriptor {
    int64_t foreign_key_id;
    int64_t child_table_id;
    int64_t parent_table_id;
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int64_t parent_index_id;
    int64_t child_index_id;
    char update_rule[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char delete_rule[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char match_option[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    uint64_t descriptor_version;
    uint64_t created_catalog_generation;
    uint64_t updated_catalog_generation;
};

struct mylite_catalog_foreign_key_column_descriptor {
    int64_t foreign_key_column_id;
    int64_t foreign_key_id;
    int64_t child_table_id;
    int64_t parent_table_id;
    int64_t child_column_id;
    int64_t parent_column_id;
    int64_t ordinal_position;
    int64_t position_in_unique_constraint;
    uint64_t descriptor_version;
    uint64_t created_catalog_generation;
    uint64_t updated_catalog_generation;
};

struct mylite_catalog_check_constraint_descriptor {
    int64_t check_constraint_id;
    int64_t table_id;
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    char check_clause[MYLITE_CATALOG_CHECK_CLAUSE_CAPACITY];
    char sqlite_expression[MYLITE_CATALOG_CHECK_CLAUSE_CAPACITY];
    bool is_enforced;
    bool name_is_generated;
    int64_t generated_ordinal;
    int64_t ordinal_position;
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
typedef int (*mylite_catalog_schema_callback)(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);
typedef int (*mylite_catalog_column_callback)(
    const struct mylite_catalog_column_descriptor *column,
    void *user_data
);
typedef int (*mylite_catalog_index_callback)(
    const struct mylite_catalog_index_descriptor *index,
    void *user_data
);
typedef int (*mylite_catalog_index_column_callback)(
    const struct mylite_catalog_index_column_descriptor *index_column,
    void *user_data
);
typedef int (*mylite_catalog_foreign_key_callback)(
    const struct mylite_catalog_foreign_key_descriptor *foreign_key,
    void *user_data
);
typedef int (*mylite_catalog_foreign_key_column_callback)(
    const struct mylite_catalog_foreign_key_column_descriptor *foreign_key_column,
    void *user_data
);
typedef int (*mylite_catalog_check_constraint_callback)(
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
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
int mylite_catalog_allocate_index_id_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t *out_index_id
);
int mylite_catalog_allocate_foreign_key_id_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t *out_foreign_key_id
);
int mylite_catalog_allocate_check_constraint_id_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t *out_check_constraint_id
);
int mylite_catalog_insert_table_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t schema_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_table_kind kind,
    int64_t auto_increment_next,
    const char *default_charset,
    const char *default_collation,
    const char *comment,
    int64_t created_time_utc_epoch,
    int64_t updated_time_utc_epoch,
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
    bool is_visible,
    bool is_auto_increment,
    enum mylite_catalog_column_default_kind default_kind,
    int64_t default_integer,
    const char *default_text,
    bool on_update_current_timestamp,
    const char *character_set_name,
    const char *collation_name,
    struct mylite_catalog_column_descriptor *out_column
);
int mylite_catalog_insert_index_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t index_id,
    int64_t table_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_index_kind kind,
    bool is_unique,
    bool is_visible,
    struct mylite_catalog_index_descriptor *out_index
);
int mylite_catalog_insert_index_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t index_id,
    int64_t table_id,
    int64_t column_id,
    int64_t ordinal_position,
    const int64_t *prefix_length,
    enum mylite_catalog_index_sort_direction sort_direction,
    struct mylite_catalog_index_column_descriptor *out_index_column
);
int mylite_catalog_insert_foreign_key_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t foreign_key_id,
    int64_t child_table_id,
    int64_t parent_table_id,
    const char *name,
    int64_t parent_index_id,
    int64_t child_index_id,
    const char *update_rule,
    const char *delete_rule,
    const char *match_option,
    struct mylite_catalog_foreign_key_descriptor *out_foreign_key
);
int mylite_catalog_insert_foreign_key_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t foreign_key_id,
    int64_t child_table_id,
    int64_t parent_table_id,
    int64_t child_column_id,
    int64_t parent_column_id,
    int64_t ordinal_position,
    int64_t position_in_unique_constraint,
    struct mylite_catalog_foreign_key_column_descriptor *out_foreign_key_column
);
int mylite_catalog_insert_check_constraint_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t check_constraint_id,
    int64_t table_id,
    const char *name,
    const char *physical_name,
    const char *check_clause,
    const char *sqlite_expression,
    bool is_enforced,
    bool name_is_generated,
    int64_t generated_ordinal,
    int64_t ordinal_position,
    struct mylite_catalog_check_constraint_descriptor *out_check_constraint
);
int mylite_catalog_delete_check_constraint_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t check_constraint_id
);
int mylite_catalog_update_check_constraint_enforcement_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t check_constraint_id,
    bool is_enforced
);
int mylite_catalog_delete_index_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t index_id
);
int mylite_catalog_delete_index_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t index_id,
    int64_t column_id,
    int64_t ordinal_position
);
int mylite_catalog_rename_index_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t index_id,
    const char *name
);
int mylite_catalog_set_index_visibility_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t index_id,
    bool is_visible
);
int mylite_catalog_delete_foreign_keys_for_child_table_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t child_table_id
);
int mylite_catalog_delete_foreign_key_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t child_table_id,
    int64_t foreign_key_id
);
int mylite_catalog_delete_check_constraints_for_table_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id
);
int mylite_catalog_rename_generated_check_constraints_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    const char *table_name
);
int mylite_catalog_delete_table_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id
);
int mylite_catalog_delete_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t column_id,
    int64_t ordinal_position
);
int mylite_catalog_rename_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t column_id,
    const char *name
);
int mylite_catalog_replace_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t column_id,
    const char *name,
    const char *logical_type,
    const char *physical_type,
    bool is_nullable,
    bool is_visible,
    bool is_auto_increment,
    enum mylite_catalog_column_default_kind default_kind,
    int64_t default_integer,
    const char *default_text,
    bool on_update_current_timestamp,
    const char *character_set_name,
    const char *collation_name
);

struct mylite_catalog_column_reorder {
    int64_t table_id;
    const struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
    int64_t metadata_replaced_column_id;
};

int mylite_catalog_reorder_columns_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct mylite_catalog_column_reorder *reorder
);
int mylite_catalog_set_column_visibility_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t column_id,
    bool is_visible
);
int mylite_catalog_delete_schema_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t schema_id
);
int mylite_catalog_update_table_identity_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table
);
int mylite_catalog_update_table_default_charset_collation_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    const char *default_charset,
    const char *default_collation,
    struct mylite_catalog_table_descriptor *out_table
);
int mylite_catalog_update_schema_default_charset_collation_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    int64_t schema_id,
    const char *default_charset,
    const char *default_collation,
    struct mylite_catalog_schema_descriptor *out_schema
);
int mylite_catalog_update_table_auto_increment_next(
    struct mylite_db *database,
    int64_t table_id,
    int64_t auto_increment_next
);
int mylite_catalog_update_table_updated_time(
    struct mylite_db *database,
    int64_t table_id,
    int64_t updated_time_utc_epoch
);
int mylite_catalog_for_each_schema(
    struct mylite_db *database,
    mylite_catalog_schema_callback callback,
    void *user_data
);
int mylite_catalog_for_each_table_in_schema(
    struct mylite_db *database,
    int64_t schema_id,
    mylite_catalog_table_callback callback,
    void *user_data
);
int mylite_catalog_for_each_column_in_table(
    struct mylite_db *database,
    int64_t table_id,
    mylite_catalog_column_callback callback,
    void *user_data
);
int mylite_catalog_for_each_index_in_table(
    struct mylite_db *database,
    int64_t table_id,
    mylite_catalog_index_callback callback,
    void *user_data
);
int mylite_catalog_for_each_index_column_in_index(
    struct mylite_db *database,
    int64_t index_id,
    mylite_catalog_index_column_callback callback,
    void *user_data
);
int mylite_catalog_for_each_foreign_key_in_child_table(
    struct mylite_db *database,
    int64_t child_table_id,
    mylite_catalog_foreign_key_callback callback,
    void *user_data
);
int mylite_catalog_for_each_foreign_key_for_parent_table(
    struct mylite_db *database,
    int64_t parent_table_id,
    mylite_catalog_foreign_key_callback callback,
    void *user_data
);
int mylite_catalog_for_each_foreign_key_column_in_foreign_key(
    struct mylite_db *database,
    int64_t foreign_key_id,
    mylite_catalog_foreign_key_column_callback callback,
    void *user_data
);
int mylite_catalog_for_each_check_constraint_in_table(
    struct mylite_db *database,
    int64_t table_id,
    mylite_catalog_check_constraint_callback callback,
    void *user_data
);
int mylite_catalog_for_each_check_constraint_in_schema(
    struct mylite_db *database,
    int64_t schema_id,
    mylite_catalog_check_constraint_callback callback,
    void *user_data
);
int mylite_catalog_try_read_check_constraint_by_physical_name(
    struct mylite_db *database,
    int64_t table_id,
    const char *physical_name,
    struct mylite_catalog_check_constraint_descriptor *out_check_constraint,
    bool *out_found
);
int mylite_catalog_try_read_primary_index_by_table_id(
    struct mylite_db *database,
    int64_t table_id,
    struct mylite_catalog_index_descriptor *out_index,
    bool *out_found
);

int mylite_catalog_create_schema(
    struct mylite_db *database,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema
);
int mylite_catalog_create_schema_with_defaults(
    struct mylite_db *database,
    const char *name,
    const char *default_charset,
    const char *default_collation,
    struct mylite_catalog_schema_descriptor *out_schema
);
int mylite_catalog_read_schema_by_name(
    struct mylite_db *database,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema
);
int mylite_catalog_try_read_schema_by_name(
    struct mylite_db *database,
    const char *name,
    struct mylite_catalog_schema_descriptor *out_schema,
    bool *out_found
);
int mylite_catalog_delete_schema(struct mylite_db *database, int64_t schema_id);

int mylite_catalog_create_table(
    struct mylite_db *database,
    int64_t schema_id,
    const char *name,
    const char *physical_name,
    enum mylite_catalog_table_kind kind,
    const char *default_charset,
    const char *default_collation,
    const char *comment,
    int64_t created_time_utc_epoch,
    int64_t updated_time_utc_epoch,
    struct mylite_catalog_table_descriptor *out_table
);
int mylite_catalog_read_table_by_name(
    struct mylite_db *database,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table
);
int mylite_catalog_try_read_table_by_name(
    struct mylite_db *database,
    int64_t schema_id,
    const char *name,
    struct mylite_catalog_table_descriptor *out_table,
    bool *out_found
);
int mylite_catalog_read_table_by_id(
    struct mylite_db *database,
    int64_t table_id,
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
    enum mylite_catalog_column_default_kind default_kind,
    int64_t default_integer,
    const char *default_text,
    bool on_update_current_timestamp,
    const char *character_set_name,
    const char *collation_name,
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
