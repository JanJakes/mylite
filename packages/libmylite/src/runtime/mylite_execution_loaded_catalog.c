#include "mylite_execution_loaded_catalog.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_execution_scalar.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct load_columns_context {
    struct mylite_catalog_column_descriptor *columns;
    size_t count;
    size_t capacity;
};

struct load_primary_key_column_context {
    struct mylite_db *database;
    const struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
    struct loaded_index_part *parts;
    size_t count;
    size_t capacity;
};

struct load_index_infos_context {
    struct mylite_db *database;
    const struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
    struct loaded_index_info *indexes;
    size_t count;
    size_t capacity;
};

struct load_foreign_key_infos_context {
    struct mylite_db *database;
    const struct mylite_catalog_column_descriptor *child_columns;
    size_t child_column_count;
    struct loaded_foreign_key_info *foreign_keys;
    size_t count;
    size_t capacity;
};

struct load_check_constraint_infos_context {
    struct mylite_db *database;
    struct loaded_check_constraint_info *check_constraints;
    size_t count;
    size_t capacity;
};

struct load_single_foreign_key_column_context {
    struct mylite_db *database;
    const struct mylite_catalog_column_descriptor *child_columns;
    size_t child_column_count;
    const struct mylite_catalog_column_descriptor *parent_columns;
    size_t parent_column_count;
    struct loaded_foreign_key_part *parts;
    size_t count;
    size_t capacity;
};

struct load_single_index_column_context {
    struct mylite_db *database;
    const struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
    struct loaded_index_part *parts;
    size_t count;
    size_t capacity;
};

struct secondary_index_presence_context {
    bool has_secondary_index;
};

struct check_constraint_presence_context {
    bool has_check_constraint;
};

static int append_loaded_column(
    const struct mylite_catalog_column_descriptor *column,
    void *user_data
);
static int append_loaded_primary_key_column(
    const struct mylite_catalog_index_column_descriptor *index_column,
    void *user_data
);
static int append_loaded_index_info(
    const struct mylite_catalog_index_descriptor *index,
    void *user_data
);
static int append_loaded_foreign_key_info(
    const struct mylite_catalog_foreign_key_descriptor *foreign_key,
    void *user_data
);
static int append_loaded_check_constraint_info(
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
    void *user_data
);
static int load_index_parts(
    struct mylite_db *database,
    int64_t index_id,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct loaded_index_part **out_parts,
    size_t *out_part_count
);
static int load_foreign_key_parts(
    struct mylite_db *database,
    const struct mylite_catalog_foreign_key_descriptor *foreign_key,
    const struct mylite_catalog_column_descriptor *child_columns,
    size_t child_column_count,
    const struct mylite_catalog_column_descriptor *parent_columns,
    size_t parent_column_count,
    struct loaded_foreign_key_part **out_parts,
    size_t *out_part_count
);
static int append_loaded_index_part(
    const struct mylite_catalog_index_column_descriptor *index_column,
    void *user_data
);
static int append_loaded_foreign_key_part(
    const struct mylite_catalog_foreign_key_column_descriptor *foreign_key_column,
    void *user_data
);
static int reserve_loaded_index_parts(
    struct load_single_index_column_context *context,
    size_t required_capacity
);
static int reserve_loaded_foreign_key_parts(
    struct load_single_foreign_key_column_context *context,
    size_t required_capacity
);
static int reserve_loaded_index_infos(
    struct load_index_infos_context *context,
    size_t required_capacity
);
static int reserve_loaded_foreign_key_infos(
    struct load_foreign_key_infos_context *context,
    size_t required_capacity
);
static int reserve_loaded_check_constraint_infos(
    struct load_check_constraint_infos_context *context,
    size_t required_capacity
);
static bool column_has_first_composite_unique_secondary_index(
    struct loaded_index_info_span indexes,
    int64_t column_id
);
static bool column_has_first_nonunique_secondary_index(
    struct loaded_index_info_span indexes,
    int64_t column_id
);
static bool column_has_first_fulltext_index(
    struct loaded_index_info_span indexes,
    int64_t column_id
);
static bool column_has_spatial_index(struct loaded_index_info_span indexes, int64_t column_id);
static bool column_is_first_not_null_unique_secondary_index(
    struct loaded_index_info_span indexes,
    int64_t column_id
);
static int note_secondary_index_presence(
    const struct mylite_catalog_index_descriptor *index,
    void *user_data
);
static int note_check_constraint_presence(
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
    void *user_data
);
static int load_columns_reserve(struct load_columns_context *context, size_t required_capacity);
static int load_table_columns_uncached(
    struct mylite_db *database,
    int64_t table_id,
    struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count
);
static int copy_loaded_table_columns(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count
);
static void maybe_cache_loaded_table_columns(
    struct mylite_db *database,
    int64_t table_id,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
);
static struct loaded_table_columns_cache_entry *find_table_columns_cache_entry(
    struct mylite_db *database,
    int64_t table_id
);
static struct loaded_table_columns_cache_entry *prepare_table_columns_cache_entry(
    struct mylite_db *database
);
static void loaded_table_columns_cache_entry_deinit(struct loaded_table_columns_cache_entry *entry);
static struct loaded_table_key_metadata_cache_entry *find_table_key_metadata_cache_entry(
    struct mylite_db *database,
    int64_t table_id
);
static struct loaded_table_key_metadata_cache_entry *prepare_table_key_metadata_cache_entry(
    struct mylite_db *database
);
static void loaded_table_key_metadata_cache_entry_deinit(
    struct loaded_table_key_metadata_cache_entry *entry
);

int mylite_execution_load_table_columns(
    struct mylite_db *database,
    int64_t table_id,
    struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count
) {
    struct loaded_table_columns_cache_entry *entry = NULL;
    int rc = MYLITE_OK;

    if (database == NULL || out_columns == NULL || out_column_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_columns = NULL;
    *out_column_count = 0U;
    if (table_id < 0) {
        return load_table_columns_uncached(database, table_id, out_columns, out_column_count);
    }

    entry = find_table_columns_cache_entry(database, table_id);
    if (entry != NULL) {
        rc = copy_loaded_table_columns(
            entry->columns,
            entry->column_count,
            out_columns,
            out_column_count
        );
        if (rc == MYLITE_NOMEM) {
            mylite_execution_set_nomem_error(database);
        }
        return rc;
    }

    rc = load_table_columns_uncached(database, table_id, out_columns, out_column_count);
    if (rc == MYLITE_OK) {
        maybe_cache_loaded_table_columns(database, table_id, *out_columns, *out_column_count);
    }

    return rc;
}

void mylite_execution_table_columns_cache_invalidate(struct mylite_db *database) {
    mylite_execution_table_columns_cache_deinit(database);
}

void mylite_execution_table_columns_cache_deinit(struct mylite_db *database) {
    if (database == NULL) {
        return;
    }

    for (size_t index = 0U; index < database->table_columns_cache_count; ++index) {
        loaded_table_columns_cache_entry_deinit(&database->table_columns_cache[index]);
    }
    database->table_columns_cache_count = 0U;
}

static int load_table_columns_uncached(
    struct mylite_db *database,
    int64_t table_id,
    struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count
) {
    struct load_columns_context context = {0};
    int rc = MYLITE_OK;

    *out_columns = NULL;
    *out_column_count = 0U;
    if (table_id < 0) {
        rc = mylite_temporary_catalog_for_each_column_in_table(
            &database->session.temporary_catalog,
            table_id,
            append_loaded_column,
            &context
        );
    } else {
        rc = mylite_catalog_for_each_column_in_table(
            database,
            table_id,
            append_loaded_column,
            &context
        );
    }
    if (rc != MYLITE_OK) {
        free(context.columns);
        if (rc == MYLITE_NOMEM) {
            mylite_execution_set_nomem_error(database);
        } else {
            mylite_execution_set_runtime_error(database, "failed to load table columns");
        }
        return rc;
    }
    if (context.count == 0U) {
        free(context.columns);
        mylite_execution_set_runtime_error(database, "table descriptor has no columns");
        return MYLITE_ERROR;
    }

    *out_columns = context.columns;
    *out_column_count = context.count;

    return MYLITE_OK;
}

static int copy_loaded_table_columns(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count
) {
    struct mylite_catalog_column_descriptor *copy = NULL;

    *out_columns = NULL;
    *out_column_count = 0U;
    if (columns == NULL || column_count == 0U) {
        return MYLITE_MISUSE;
    }
    if (column_count > SIZE_MAX / sizeof(*copy)) {
        return MYLITE_NOMEM;
    }
    copy = (struct mylite_catalog_column_descriptor *)malloc(column_count * sizeof(*copy));
    if (copy == NULL) {
        return MYLITE_NOMEM;
    }
    memcpy(copy, columns, column_count * sizeof(*copy));
    *out_columns = copy;
    *out_column_count = column_count;
    return MYLITE_OK;
}

static void maybe_cache_loaded_table_columns(
    struct mylite_db *database,
    int64_t table_id,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
) {
    struct loaded_table_columns_cache_entry *entry = NULL;
    struct mylite_catalog_column_descriptor *copy = NULL;

    if (database == NULL || columns == NULL || column_count == 0U || table_id < 0 ||
        column_count > SIZE_MAX / sizeof(*copy)) {
        return;
    }

    copy = (struct mylite_catalog_column_descriptor *)malloc(column_count * sizeof(*copy));
    if (copy == NULL) {
        return;
    }
    memcpy(copy, columns, column_count * sizeof(*copy));

    entry = prepare_table_columns_cache_entry(database);
    if (entry == NULL) {
        free(copy);
        return;
    }

    *entry = (struct loaded_table_columns_cache_entry){
        .is_valid = true,
        .table_id = table_id,
        .catalog_generation = database->session.catalog_generation,
        .sqlite_schema_generation = database->session.sqlite_schema_generation,
        .columns = copy,
        .column_count = column_count,
    };
}

struct loaded_table_key_metadata mylite_execution_loaded_table_key_metadata_init(void) {
    return (struct loaded_table_key_metadata){
        .primary_key = mylite_execution_primary_key_info_init(),
        .indexes = NULL,
        .index_count = 0U,
    };
}

void mylite_execution_loaded_table_key_metadata_deinit(struct loaded_table_key_metadata *metadata) {
    if (metadata == NULL) {
        return;
    }

    mylite_execution_primary_key_info_deinit(&metadata->primary_key);
    mylite_execution_loaded_index_infos_deinit(&metadata->indexes, &metadata->index_count);
    *metadata = mylite_execution_loaded_table_key_metadata_init();
}

int mylite_execution_load_table_key_metadata(
    struct mylite_db *database,
    int64_t table_id,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct loaded_table_key_metadata *out_metadata
) {
    int rc = MYLITE_OK;

    if (out_metadata == NULL) {
        return MYLITE_MISUSE;
    }
    *out_metadata = mylite_execution_loaded_table_key_metadata_init();

    rc = mylite_execution_load_primary_key_info(
        database,
        table_id,
        columns,
        column_count,
        &out_metadata->primary_key
    );
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
            database,
            table_id,
            columns,
            column_count,
            &out_metadata->indexes,
            &out_metadata->index_count
        );
    }
    if (rc != MYLITE_OK) {
        mylite_execution_loaded_table_key_metadata_deinit(out_metadata);
    }

    return rc;
}

int mylite_execution_borrow_cached_table_key_metadata(
    struct mylite_db *database,
    int64_t table_id,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const struct loaded_table_key_metadata **out_metadata
) {
    struct loaded_table_key_metadata_cache_entry *entry = NULL;
    struct loaded_table_key_metadata metadata = mylite_execution_loaded_table_key_metadata_init();
    int rc = MYLITE_OK;

    if (database == NULL || columns == NULL || column_count == 0U || out_metadata == NULL) {
        return MYLITE_MISUSE;
    }
    *out_metadata = NULL;
    if (table_id < 0) {
        return MYLITE_MISUSE;
    }

    entry = find_table_key_metadata_cache_entry(database, table_id);
    if (entry != NULL) {
        *out_metadata = &entry->metadata;
        return MYLITE_OK;
    }

    rc = mylite_execution_load_table_key_metadata(
        database,
        table_id,
        columns,
        column_count,
        &metadata
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    entry = prepare_table_key_metadata_cache_entry(database);
    if (entry == NULL) {
        mylite_execution_loaded_table_key_metadata_deinit(&metadata);
        return MYLITE_NOMEM;
    }

    *entry = (struct loaded_table_key_metadata_cache_entry){
        .is_valid = true,
        .table_id = table_id,
        .catalog_generation = database->session.catalog_generation,
        .sqlite_schema_generation = database->session.sqlite_schema_generation,
        .metadata = metadata,
    };
    *out_metadata = &entry->metadata;

    return MYLITE_OK;
}

void mylite_execution_table_key_metadata_cache_invalidate(struct mylite_db *database) {
    mylite_execution_table_key_metadata_cache_deinit(database);
}

void mylite_execution_table_key_metadata_cache_deinit(struct mylite_db *database) {
    if (database == NULL) {
        return;
    }

    for (size_t index = 0U; index < database->table_key_metadata_cache_count; ++index) {
        loaded_table_key_metadata_cache_entry_deinit(&database->table_key_metadata_cache[index]);
    }
    database->table_key_metadata_cache_count = 0U;
}

static int append_loaded_column(
    const struct mylite_catalog_column_descriptor *column,
    void *user_data
) {
    struct load_columns_context *context = user_data;
    int rc = MYLITE_OK;

    if (column == NULL || context == NULL) {
        return MYLITE_MISUSE;
    }

    rc = load_columns_reserve(context, context->count + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }

    context->columns[context->count] = *column;
    ++context->count;

    return MYLITE_OK;
}

static struct loaded_table_columns_cache_entry *find_table_columns_cache_entry(
    struct mylite_db *database,
    int64_t table_id
) {
    if (database == NULL) {
        return NULL;
    }

    for (size_t index = 0U; index < database->table_columns_cache_count; ++index) {
        struct loaded_table_columns_cache_entry *entry = &database->table_columns_cache[index];

        if (entry->is_valid && entry->table_id == table_id &&
            entry->catalog_generation == database->session.catalog_generation &&
            entry->sqlite_schema_generation == database->session.sqlite_schema_generation) {
            return entry;
        }
    }

    return NULL;
}

static struct loaded_table_columns_cache_entry *prepare_table_columns_cache_entry(
    struct mylite_db *database
) {
    struct loaded_table_columns_cache_entry *entry = NULL;

    if (database == NULL) {
        return NULL;
    }
    if (database->table_columns_cache_count < MYLITE_EXECUTION_TABLE_COLUMNS_CACHE_LIMIT) {
        entry = &database->table_columns_cache[database->table_columns_cache_count];
        ++database->table_columns_cache_count;
        return entry;
    }

    entry = &database->table_columns_cache[0];
    loaded_table_columns_cache_entry_deinit(entry);
    return entry;
}

static void loaded_table_columns_cache_entry_deinit(struct loaded_table_columns_cache_entry *entry
) {
    if (entry == NULL) {
        return;
    }

    free(entry->columns);
    *entry = (struct loaded_table_columns_cache_entry){0};
}

static struct loaded_table_key_metadata_cache_entry *find_table_key_metadata_cache_entry(
    struct mylite_db *database,
    int64_t table_id
) {
    if (database == NULL) {
        return NULL;
    }

    for (size_t index = 0U; index < database->table_key_metadata_cache_count; ++index) {
        struct loaded_table_key_metadata_cache_entry *entry =
            &database->table_key_metadata_cache[index];

        if (entry->is_valid && entry->table_id == table_id &&
            entry->catalog_generation == database->session.catalog_generation &&
            entry->sqlite_schema_generation == database->session.sqlite_schema_generation) {
            return entry;
        }
    }

    return NULL;
}

static struct loaded_table_key_metadata_cache_entry *prepare_table_key_metadata_cache_entry(
    struct mylite_db *database
) {
    struct loaded_table_key_metadata_cache_entry *entry = NULL;

    if (database == NULL) {
        return NULL;
    }
    if (database->table_key_metadata_cache_count <
        MYLITE_EXECUTION_TABLE_KEY_METADATA_CACHE_LIMIT) {
        entry = &database->table_key_metadata_cache[database->table_key_metadata_cache_count];
        ++database->table_key_metadata_cache_count;
        return entry;
    }

    entry = &database->table_key_metadata_cache[0];
    loaded_table_key_metadata_cache_entry_deinit(entry);
    return entry;
}

static void loaded_table_key_metadata_cache_entry_deinit(
    struct loaded_table_key_metadata_cache_entry *entry
) {
    if (entry == NULL) {
        return;
    }

    mylite_execution_loaded_table_key_metadata_deinit(&entry->metadata);
    *entry = (struct loaded_table_key_metadata_cache_entry){0};
}

struct primary_key_info mylite_execution_primary_key_info_init(void) {
    return (struct primary_key_info){
        .has_primary_key = false,
        .index = {.is_unique = false},
        .parts = NULL,
        .part_count = 0U,
    };
}

void mylite_execution_primary_key_info_deinit(struct primary_key_info *info) {
    if (info == NULL) {
        return;
    }
    free(info->parts);
    *info = mylite_execution_primary_key_info_init();
}

int mylite_execution_load_primary_key_info(
    struct mylite_db *database,
    int64_t table_id,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct primary_key_info *out_info
) {
    struct load_primary_key_column_context context = {0};
    bool found = false;
    int rc = MYLITE_OK;

    if (out_info == NULL) {
        return MYLITE_MISUSE;
    }
    *out_info = mylite_execution_primary_key_info_init();

    if (table_id < 0) {
        rc = mylite_temporary_catalog_try_read_primary_index_by_table_id(
            &database->session.temporary_catalog,
            table_id,
            &out_info->index,
            &found
        );
    } else {
        rc = mylite_catalog_try_read_primary_index_by_table_id(
            database,
            table_id,
            &out_info->index,
            &found
        );
    }
    if (rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(database, "failed to load primary-key descriptor");
        return rc;
    }
    if (!found) {
        return MYLITE_OK;
    }

    context.database = database;
    context.columns = columns;
    context.column_count = column_count;
    if (table_id < 0) {
        rc = mylite_temporary_catalog_for_each_index_column_in_index(
            &database->session.temporary_catalog,
            out_info->index.index_id,
            append_loaded_primary_key_column,
            &context
        );
    } else {
        rc = mylite_catalog_for_each_index_column_in_index(
            database,
            out_info->index.index_id,
            append_loaded_primary_key_column,
            &context
        );
    }
    if (rc != MYLITE_OK || context.count == 0U) {
        free(context.parts);
        mylite_execution_set_runtime_error(database, "invalid primary-key column descriptor");
        return rc == MYLITE_OK ? MYLITE_ERROR : rc;
    }

    out_info->has_primary_key = true;
    out_info->parts = context.parts;
    out_info->part_count = context.count;

    return MYLITE_OK;
}

int mylite_execution_load_table_index_infos(
    struct mylite_db *database,
    int64_t table_id,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct loaded_index_info **out_indexes,
    size_t *out_index_count
) {
    struct load_index_infos_context context = {
        .database = database,
        .columns = columns,
        .column_count = column_count,
        .indexes = NULL,
        .count = 0U,
        .capacity = 0U,
    };
    int rc = MYLITE_OK;

    if (out_indexes == NULL || out_index_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_indexes = NULL;
    *out_index_count = 0U;

    if (table_id < 0) {
        rc = mylite_temporary_catalog_for_each_index_in_table(
            &database->session.temporary_catalog,
            table_id,
            append_loaded_index_info,
            &context
        );
    } else {
        rc = mylite_catalog_for_each_index_in_table(
            database,
            table_id,
            append_loaded_index_info,
            &context
        );
    }
    if (rc != MYLITE_OK) {
        mylite_execution_loaded_index_infos_deinit(&context.indexes, &context.count);
        if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
            mylite_execution_set_runtime_error(database, "failed to load index descriptors");
        }
        return rc;
    }

    *out_indexes = context.indexes;
    *out_index_count = context.count;
    return MYLITE_OK;
}

void mylite_execution_loaded_index_infos_deinit(
    struct loaded_index_info **indexes,
    size_t *index_count
) {
    if (indexes == NULL || index_count == NULL) {
        return;
    }
    for (size_t index = 0U; index < *index_count; ++index) {
        mylite_execution_loaded_index_info_deinit(&(*indexes)[index]);
    }
    free(*indexes);
    *indexes = NULL;
    *index_count = 0U;
}

void mylite_execution_loaded_index_info_deinit(struct loaded_index_info *index) {
    if (index == NULL) {
        return;
    }
    free(index->parts);
    *index = (struct loaded_index_info){0};
}

int mylite_execution_load_table_foreign_key_infos(
    struct mylite_db *database,
    int64_t table_id,
    const struct mylite_catalog_column_descriptor *child_columns,
    size_t child_column_count,
    struct loaded_foreign_key_info **out_foreign_keys,
    size_t *out_foreign_key_count
) {
    struct load_foreign_key_infos_context context = {
        .database = database,
        .child_columns = child_columns,
        .child_column_count = child_column_count,
        .foreign_keys = NULL,
        .count = 0U,
        .capacity = 0U,
    };
    int rc = MYLITE_OK;

    if (out_foreign_keys == NULL || out_foreign_key_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_foreign_keys = NULL;
    *out_foreign_key_count = 0U;
    if (table_id < 0) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_for_each_foreign_key_in_child_table(
        database,
        table_id,
        append_loaded_foreign_key_info,
        &context
    );
    if (rc != MYLITE_OK) {
        mylite_execution_loaded_foreign_key_infos_deinit(&context.foreign_keys, &context.count);
        if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
            mylite_execution_set_runtime_error(database, "failed to load foreign-key descriptors");
        }
        return rc;
    }

    *out_foreign_keys = context.foreign_keys;
    *out_foreign_key_count = context.count;
    return MYLITE_OK;
}

int mylite_execution_load_parent_foreign_key_infos(
    struct mylite_db *database,
    int64_t parent_table_id,
    struct loaded_foreign_key_info **out_foreign_keys,
    size_t *out_foreign_key_count
) {
    struct load_foreign_key_infos_context context = {
        .database = database,
        .child_columns = NULL,
        .child_column_count = 0U,
        .foreign_keys = NULL,
        .count = 0U,
        .capacity = 0U,
    };
    int rc = MYLITE_OK;

    if (out_foreign_keys == NULL || out_foreign_key_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_foreign_keys = NULL;
    *out_foreign_key_count = 0U;
    if (parent_table_id <= 0) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_for_each_foreign_key_for_parent_table(
        database,
        parent_table_id,
        append_loaded_foreign_key_info,
        &context
    );
    if (rc != MYLITE_OK) {
        mylite_execution_loaded_foreign_key_infos_deinit(&context.foreign_keys, &context.count);
        if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
            mylite_execution_set_runtime_error(
                database,
                "failed to load parent foreign-key descriptors"
            );
        }
        return rc;
    }

    *out_foreign_keys = context.foreign_keys;
    *out_foreign_key_count = context.count;
    return MYLITE_OK;
}

void mylite_execution_loaded_foreign_key_infos_deinit(
    struct loaded_foreign_key_info **foreign_keys,
    size_t *foreign_key_count
) {
    if (foreign_keys == NULL || foreign_key_count == NULL) {
        return;
    }
    for (size_t index = 0U; index < *foreign_key_count; ++index) {
        mylite_execution_loaded_foreign_key_info_deinit(&(*foreign_keys)[index]);
    }
    free(*foreign_keys);
    *foreign_keys = NULL;
    *foreign_key_count = 0U;
}

void mylite_execution_loaded_foreign_key_info_deinit(struct loaded_foreign_key_info *foreign_key) {
    if (foreign_key == NULL) {
        return;
    }
    free(foreign_key->parts);
    *foreign_key = (struct loaded_foreign_key_info){0};
}

int mylite_execution_load_table_check_constraint_infos(
    struct mylite_db *database,
    int64_t table_id,
    struct loaded_check_constraint_info **out_check_constraints,
    size_t *out_check_constraint_count
) {
    struct load_check_constraint_infos_context context = {
        .database = database,
        .check_constraints = NULL,
        .count = 0U,
        .capacity = 0U,
    };
    int rc = MYLITE_OK;

    if (out_check_constraints == NULL || out_check_constraint_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_check_constraints = NULL;
    *out_check_constraint_count = 0U;
    if (table_id < 0) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_for_each_check_constraint_in_table(
        database,
        table_id,
        append_loaded_check_constraint_info,
        &context
    );
    if (rc != MYLITE_OK) {
        mylite_execution_loaded_check_constraint_infos_deinit(
            &context.check_constraints,
            &context.count
        );
        if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
            mylite_execution_set_runtime_error(
                database,
                "failed to load CHECK constraint descriptors"
            );
        }
        return rc;
    }

    *out_check_constraints = context.check_constraints;
    *out_check_constraint_count = context.count;
    return MYLITE_OK;
}

void mylite_execution_loaded_check_constraint_infos_deinit(
    struct loaded_check_constraint_info **check_constraints,
    size_t *check_constraint_count
) {
    if (check_constraints == NULL || check_constraint_count == NULL) {
        return;
    }
    free(*check_constraints);
    *check_constraints = NULL;
    *check_constraint_count = 0U;
}

static int append_loaded_index_info(
    const struct mylite_catalog_index_descriptor *index,
    void *user_data
) {
    struct load_index_infos_context *context = user_data;
    struct loaded_index_info *loaded = NULL;
    struct loaded_index_part *parts = NULL;
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (index == NULL || context == NULL || context->database == NULL) {
        return MYLITE_MISUSE;
    }

    rc = load_index_parts(
        context->database,
        index->index_id,
        context->columns,
        context->column_count,
        &parts,
        &part_count
    );
    if (rc == MYLITE_OK) {
        rc = reserve_loaded_index_infos(context, context->count + 1U);
    }
    if (rc != MYLITE_OK) {
        free(parts);
        return rc;
    }

    loaded = &context->indexes[context->count];
    *loaded = (struct loaded_index_info){
        .index = *index,
        .parts = parts,
        .part_count = part_count,
    };
    ++context->count;
    return MYLITE_OK;
}

int mylite_execution_load_foreign_key_info(
    struct mylite_db *database,
    const struct mylite_catalog_foreign_key_descriptor *foreign_key,
    const struct mylite_catalog_column_descriptor *provided_child_columns,
    size_t provided_child_column_count,
    struct loaded_foreign_key_info *out_info
) {
    struct mylite_catalog_column_descriptor *owned_child_columns = NULL;
    struct mylite_catalog_column_descriptor *parent_columns = NULL;
    struct loaded_index_info *parent_indexes = NULL;
    const struct mylite_catalog_column_descriptor *child_columns = provided_child_columns;
    struct loaded_foreign_key_part *parts = NULL;
    size_t child_column_count = provided_child_column_count;
    size_t parent_column_count = 0U;
    size_t parent_index_count = 0U;
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    *out_info = (struct loaded_foreign_key_info){.foreign_key = *foreign_key};
    rc = mylite_catalog_read_table_by_id(
        database,
        foreign_key->child_table_id,
        &out_info->child_table
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_id(
            database,
            foreign_key->parent_table_id,
            &out_info->parent_table
        );
    }
    if (rc == MYLITE_OK && child_columns == NULL) {
        rc = mylite_execution_load_table_columns(
            database,
            out_info->child_table.table_id,
            &owned_child_columns,
            &child_column_count
        );
        child_columns = owned_child_columns;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_columns(
            database,
            out_info->parent_table.table_id,
            &parent_columns,
            &parent_column_count
        );
    }
    if (rc == MYLITE_OK) {
        bool found_parent_index = false;

        rc = mylite_execution_load_table_index_infos(
            database,
            out_info->parent_table.table_id,
            parent_columns,
            parent_column_count,
            &parent_indexes,
            &parent_index_count
        );
        for (size_t index = 0U; rc == MYLITE_OK && index < parent_index_count; ++index) {
            if (parent_indexes[index].index.index_id == foreign_key->parent_index_id) {
                out_info->parent_index = parent_indexes[index].index;
                found_parent_index = true;
                break;
            }
        }
        if (rc == MYLITE_OK && !found_parent_index) {
            mylite_execution_set_runtime_error(
                database,
                "foreign-key parent index descriptor is stale"
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = load_foreign_key_parts(
            database,
            foreign_key,
            child_columns,
            child_column_count,
            parent_columns,
            parent_column_count,
            &parts,
            &part_count
        );
    }

    mylite_execution_loaded_index_infos_deinit(&parent_indexes, &parent_index_count);
    free(parent_columns);
    free(owned_child_columns);
    if (rc != MYLITE_OK) {
        free(parts);
        *out_info = (struct loaded_foreign_key_info){0};
        return rc;
    }

    out_info->parts = parts;
    out_info->part_count = part_count;
    return MYLITE_OK;
}

static int append_loaded_foreign_key_info(
    const struct mylite_catalog_foreign_key_descriptor *foreign_key,
    void *user_data
) {
    struct load_foreign_key_infos_context *context = user_data;
    struct loaded_foreign_key_info *loaded = NULL;
    int rc = MYLITE_OK;

    if (foreign_key == NULL || context == NULL || context->database == NULL) {
        return MYLITE_MISUSE;
    }

    rc = reserve_loaded_foreign_key_infos(context, context->count + 1U);
    if (rc == MYLITE_OK) {
        loaded = &context->foreign_keys[context->count];
        *loaded = (struct loaded_foreign_key_info){0};
        rc = mylite_execution_load_foreign_key_info(
            context->database,
            foreign_key,
            context->child_columns,
            context->child_column_count,
            loaded
        );
    }
    if (rc == MYLITE_OK) {
        ++context->count;
    } else if (loaded != NULL) {
        mylite_execution_loaded_foreign_key_info_deinit(loaded);
    }
    return rc;
}

static int append_loaded_check_constraint_info(
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
    void *user_data
) {
    struct load_check_constraint_infos_context *context = user_data;
    int rc = MYLITE_OK;

    if (check_constraint == NULL || context == NULL) {
        return MYLITE_MISUSE;
    }
    rc = reserve_loaded_check_constraint_infos(context, context->count + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }

    context->check_constraints[context->count] =
        (struct loaded_check_constraint_info){.check_constraint = *check_constraint};
    ++context->count;
    return MYLITE_OK;
}

static int load_index_parts(
    struct mylite_db *database,
    int64_t index_id,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct loaded_index_part **out_parts,
    size_t *out_part_count
) {
    struct load_single_index_column_context context = {
        .database = database,
        .columns = columns,
        .column_count = column_count,
        .parts = NULL,
        .count = 0U,
        .capacity = 0U,
    };
    int rc = MYLITE_OK;

    if (out_parts == NULL || out_part_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_parts = NULL;
    *out_part_count = 0U;

    if (index_id < 0) {
        rc = mylite_temporary_catalog_for_each_index_column_in_index(
            &database->session.temporary_catalog,
            index_id,
            append_loaded_index_part,
            &context
        );
    } else {
        rc = mylite_catalog_for_each_index_column_in_index(
            database,
            index_id,
            append_loaded_index_part,
            &context
        );
    }
    if (rc != MYLITE_OK || context.count == 0U) {
        free(context.parts);
        mylite_execution_set_runtime_error(database, "invalid index column descriptor");
        return rc == MYLITE_OK ? MYLITE_ERROR : rc;
    }

    *out_parts = context.parts;
    *out_part_count = context.count;
    return MYLITE_OK;
}

static int load_foreign_key_parts(
    struct mylite_db *database,
    const struct mylite_catalog_foreign_key_descriptor *foreign_key,
    const struct mylite_catalog_column_descriptor *child_columns,
    size_t child_column_count,
    const struct mylite_catalog_column_descriptor *parent_columns,
    size_t parent_column_count,
    struct loaded_foreign_key_part **out_parts,
    size_t *out_part_count
) {
    struct load_single_foreign_key_column_context context = {
        .database = database,
        .child_columns = child_columns,
        .child_column_count = child_column_count,
        .parent_columns = parent_columns,
        .parent_column_count = parent_column_count,
        .parts = NULL,
        .count = 0U,
        .capacity = 0U,
    };
    int rc = MYLITE_OK;

    if (out_parts == NULL || out_part_count == NULL || foreign_key == NULL) {
        return MYLITE_MISUSE;
    }
    *out_parts = NULL;
    *out_part_count = 0U;

    rc = mylite_catalog_for_each_foreign_key_column_in_foreign_key(
        database,
        foreign_key->foreign_key_id,
        append_loaded_foreign_key_part,
        &context
    );
    if (rc != MYLITE_OK || context.count == 0U) {
        free(context.parts);
        mylite_execution_set_runtime_error(database, "invalid foreign-key column descriptor");
        return rc == MYLITE_OK ? MYLITE_ERROR : rc;
    }

    *out_parts = context.parts;
    *out_part_count = context.count;
    return MYLITE_OK;
}

static int append_loaded_index_part(
    const struct mylite_catalog_index_column_descriptor *index_column,
    void *user_data
) {
    struct load_single_index_column_context *context = user_data;
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    if (index_column == NULL || context == NULL || context->database == NULL) {
        return MYLITE_MISUSE;
    }
    rc = mylite_execution_find_column_index_by_id(
        context->columns,
        context->column_count,
        &column_index,
        index_column->column_id
    );
    if (rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(context->database, "index column descriptor is stale");
        return MYLITE_ERROR;
    }
    rc = reserve_loaded_index_parts(context, context->count + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }

    context->parts[context->count] = (struct loaded_index_part){
        .index_column = *index_column,
        .column = context->columns[column_index],
        .column_index = column_index,
    };
    ++context->count;

    return MYLITE_OK;
}

static int append_loaded_foreign_key_part(
    const struct mylite_catalog_foreign_key_column_descriptor *foreign_key_column,
    void *user_data
) {
    struct load_single_foreign_key_column_context *context = user_data;
    size_t child_column_index = 0U;
    size_t parent_column_index = 0U;
    int rc = MYLITE_OK;

    if (foreign_key_column == NULL || context == NULL || context->database == NULL) {
        return MYLITE_MISUSE;
    }
    rc = mylite_execution_find_column_index_by_id(
        context->child_columns,
        context->child_column_count,
        &child_column_index,
        foreign_key_column->child_column_id
    );
    if (rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(
            context->database,
            "foreign-key child column descriptor is stale"
        );
        return MYLITE_ERROR;
    }
    rc = mylite_execution_find_column_index_by_id(
        context->parent_columns,
        context->parent_column_count,
        &parent_column_index,
        foreign_key_column->parent_column_id
    );
    if (rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(
            context->database,
            "foreign-key parent column descriptor is stale"
        );
        return MYLITE_ERROR;
    }
    rc = reserve_loaded_foreign_key_parts(context, context->count + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }

    context->parts[context->count] = (struct loaded_foreign_key_part){
        .foreign_key_column = *foreign_key_column,
        .child_column = context->child_columns[child_column_index],
        .parent_column = context->parent_columns[parent_column_index],
        .child_column_index = child_column_index,
        .parent_column_index = parent_column_index,
    };
    ++context->count;

    return MYLITE_OK;
}

static int reserve_loaded_index_parts(
    struct load_single_index_column_context *context,
    size_t required_capacity
) {
    enum { initial_loaded_index_part_capacity = 4 };

    struct loaded_index_part *parts = NULL;
    size_t capacity = 0U;

    if (required_capacity <= context->capacity) {
        return MYLITE_OK;
    }
    capacity = context->capacity == 0U ? initial_loaded_index_part_capacity : context->capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            mylite_execution_set_nomem_error(context->database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*parts)) {
        mylite_execution_set_nomem_error(context->database);
        return MYLITE_NOMEM;
    }

    parts = realloc(context->parts, capacity * sizeof(*parts));
    if (parts == NULL) {
        mylite_execution_set_nomem_error(context->database);
        return MYLITE_NOMEM;
    }

    context->parts = parts;
    context->capacity = capacity;
    return MYLITE_OK;
}

static int reserve_loaded_foreign_key_parts(
    struct load_single_foreign_key_column_context *context,
    size_t required_capacity
) {
    enum { initial_loaded_foreign_key_part_capacity = 4 };

    struct loaded_foreign_key_part *parts = NULL;
    size_t capacity = 0U;

    if (required_capacity <= context->capacity) {
        return MYLITE_OK;
    }
    capacity =
        context->capacity == 0U ? initial_loaded_foreign_key_part_capacity : context->capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            mylite_execution_set_nomem_error(context->database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*parts)) {
        mylite_execution_set_nomem_error(context->database);
        return MYLITE_NOMEM;
    }

    parts = realloc(context->parts, capacity * sizeof(*parts));
    if (parts == NULL) {
        mylite_execution_set_nomem_error(context->database);
        return MYLITE_NOMEM;
    }

    context->parts = parts;
    context->capacity = capacity;
    return MYLITE_OK;
}

static int reserve_loaded_index_infos(
    struct load_index_infos_context *context,
    size_t required_capacity
) {
    enum { initial_loaded_index_capacity = 4 };

    struct loaded_index_info *indexes = NULL;
    size_t capacity = 0U;

    if (required_capacity <= context->capacity) {
        return MYLITE_OK;
    }
    capacity = context->capacity == 0U ? initial_loaded_index_capacity : context->capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            mylite_execution_set_nomem_error(context->database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*indexes)) {
        mylite_execution_set_nomem_error(context->database);
        return MYLITE_NOMEM;
    }

    indexes = realloc(context->indexes, capacity * sizeof(*indexes));
    if (indexes == NULL) {
        mylite_execution_set_nomem_error(context->database);
        return MYLITE_NOMEM;
    }

    context->indexes = indexes;
    context->capacity = capacity;
    return MYLITE_OK;
}

static int reserve_loaded_foreign_key_infos(
    struct load_foreign_key_infos_context *context,
    size_t required_capacity
) {
    enum { initial_loaded_foreign_key_capacity = 4 };

    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t capacity = 0U;

    if (required_capacity <= context->capacity) {
        return MYLITE_OK;
    }
    capacity = context->capacity == 0U ? initial_loaded_foreign_key_capacity : context->capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            mylite_execution_set_nomem_error(context->database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*foreign_keys)) {
        mylite_execution_set_nomem_error(context->database);
        return MYLITE_NOMEM;
    }

    foreign_keys = realloc(context->foreign_keys, capacity * sizeof(*foreign_keys));
    if (foreign_keys == NULL) {
        mylite_execution_set_nomem_error(context->database);
        return MYLITE_NOMEM;
    }

    context->foreign_keys = foreign_keys;
    context->capacity = capacity;
    return MYLITE_OK;
}

static int reserve_loaded_check_constraint_infos(
    struct load_check_constraint_infos_context *context,
    size_t required_capacity
) {
    enum { initial_loaded_check_constraint_capacity = 4 };

    struct loaded_check_constraint_info *check_constraints = NULL;
    size_t capacity = 0U;

    if (required_capacity <= context->capacity) {
        return MYLITE_OK;
    }
    capacity =
        context->capacity == 0U ? initial_loaded_check_constraint_capacity : context->capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            mylite_execution_set_nomem_error(context->database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*check_constraints)) {
        mylite_execution_set_nomem_error(context->database);
        return MYLITE_NOMEM;
    }

    check_constraints = realloc(context->check_constraints, capacity * sizeof(*check_constraints));
    if (check_constraints == NULL) {
        mylite_execution_set_nomem_error(context->database);
        return MYLITE_NOMEM;
    }

    context->check_constraints = check_constraints;
    context->capacity = capacity;
    return MYLITE_OK;
}

const char *mylite_execution_column_key_text(
    struct loaded_index_info_span indexes,
    const struct primary_key_info *primary_key,
    const struct mylite_catalog_column_descriptor *column
) {
    if (mylite_execution_primary_key_info_contains_column_id(primary_key, column->column_id)) {
        return "PRI";
    }
    if ((primary_key == NULL || !primary_key->has_primary_key) &&
        column_is_first_not_null_unique_secondary_index(indexes, column->column_id)) {
        return "PRI";
    }
    if (mylite_execution_column_has_unique_secondary_index(indexes, column->column_id)) {
        return "UNI";
    }
    if (column_has_first_composite_unique_secondary_index(indexes, column->column_id)) {
        return "MUL";
    }
    if (column_has_first_nonunique_secondary_index(indexes, column->column_id)) {
        return "MUL";
    }
    if (column_has_first_fulltext_index(indexes, column->column_id)) {
        return "MUL";
    }
    if (column_has_spatial_index(indexes, column->column_id)) {
        return "MUL";
    }

    return "";
}

bool mylite_execution_primary_key_info_contains_column_id(
    const struct primary_key_info *primary_key,
    int64_t column_id
) {
    if (primary_key == NULL || !primary_key->has_primary_key) {
        return false;
    }
    for (size_t part_index = 0U; part_index < primary_key->part_count; ++part_index) {
        if (primary_key->parts[part_index].index_column.column_id == column_id) {
            return true;
        }
    }

    return false;
}

bool mylite_execution_column_has_unique_secondary_index(
    struct loaded_index_info_span indexes,
    int64_t column_id
) {
    for (size_t index = 0U; index < indexes.count; ++index) {
        if (indexes.indexes[index].index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY &&
            indexes.indexes[index].index.is_unique && indexes.indexes[index].part_count == 1U) {
            for (size_t part_index = 0U; part_index < indexes.indexes[index].part_count;
                 ++part_index) {
                if (indexes.indexes[index].parts[part_index].index_column.column_id == column_id) {
                    return true;
                }
            }
        }
    }

    return false;
}

static bool column_has_first_composite_unique_secondary_index(
    struct loaded_index_info_span indexes,
    int64_t column_id
) {
    for (size_t index = 0U; index < indexes.count; ++index) {
        if (indexes.indexes[index].index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY &&
            indexes.indexes[index].index.is_unique && indexes.indexes[index].part_count > 1U &&
            indexes.indexes[index].parts[0].index_column.column_id == column_id) {
            return true;
        }
    }

    return false;
}

static bool column_has_first_nonunique_secondary_index(
    struct loaded_index_info_span indexes,
    int64_t column_id
) {
    for (size_t index = 0U; index < indexes.count; ++index) {
        if (indexes.indexes[index].index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY &&
            !indexes.indexes[index].index.is_unique && indexes.indexes[index].part_count > 0U &&
            indexes.indexes[index].parts[0].index_column.column_id == column_id) {
            return true;
        }
    }

    return false;
}

bool mylite_execution_column_has_nonunique_secondary_index(
    struct loaded_index_info_span indexes,
    int64_t column_id
) {
    for (size_t index = 0U; index < indexes.count; ++index) {
        if (indexes.indexes[index].index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY &&
            !indexes.indexes[index].index.is_unique) {
            for (size_t part_index = 0U; part_index < indexes.indexes[index].part_count;
                 ++part_index) {
                if (indexes.indexes[index].parts[part_index].index_column.column_id == column_id) {
                    return true;
                }
            }
        }
    }

    return false;
}

static bool column_has_first_fulltext_index(
    struct loaded_index_info_span indexes,
    int64_t column_id
) {
    for (size_t index = 0U; index < indexes.count; ++index) {
        if (indexes.indexes[index].index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT &&
            indexes.indexes[index].part_count > 0U &&
            indexes.indexes[index].parts[0].index_column.column_id == column_id) {
            return true;
        }
    }

    return false;
}

static bool column_has_spatial_index(struct loaded_index_info_span indexes, int64_t column_id) {
    for (size_t index = 0U; index < indexes.count; ++index) {
        if (indexes.indexes[index].index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL &&
            indexes.indexes[index].part_count > 0U &&
            indexes.indexes[index].parts[0].index_column.column_id == column_id) {
            return true;
        }
    }

    return false;
}

static bool column_is_first_not_null_unique_secondary_index(
    struct loaded_index_info_span indexes,
    int64_t column_id
) {
    for (size_t index = 0U; index < indexes.count; ++index) {
        bool has_nullable_or_prefix_part = false;

        if (indexes.indexes[index].index.kind != MYLITE_CATALOG_INDEX_KIND_SECONDARY ||
            !indexes.indexes[index].index.is_unique || indexes.indexes[index].part_count == 0U) {
            continue;
        }

        for (size_t part_index = 0U; part_index < indexes.indexes[index].part_count; ++part_index) {
            if (indexes.indexes[index].parts[part_index].index_column.has_prefix_length ||
                indexes.indexes[index].parts[part_index].column.is_nullable) {
                has_nullable_or_prefix_part = true;
                break;
            }
        }
        if (has_nullable_or_prefix_part) {
            continue;
        }
        for (size_t part_index = 0U; part_index < indexes.indexes[index].part_count; ++part_index) {
            if (indexes.indexes[index].parts[part_index].index_column.column_id == column_id) {
                return true;
            }
        }
        return false;
    }

    return false;
}

int mylite_execution_reject_primary_key_table_alter(
    struct mylite_db *database,
    int64_t table_id,
    const char *message
) {
    struct mylite_catalog_index_descriptor index = {0};
    bool found = false;
    int rc = mylite_catalog_try_read_primary_index_by_table_id(database, table_id, &index, &found);

    if (rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(database, "failed to load primary-key descriptor");
        return rc;
    }
    if (found) {
        mylite_execution_set_unsupported_error(database, message);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

int mylite_execution_reject_secondary_index_table_alter(
    struct mylite_db *database,
    int64_t table_id,
    const char *message
) {
    struct secondary_index_presence_context context = {.has_secondary_index = false};
    int rc = mylite_catalog_for_each_index_in_table(
        database,
        table_id,
        note_secondary_index_presence,
        &context
    );

    if (rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(database, "failed to load secondary-index descriptors");
        return rc;
    }
    if (context.has_secondary_index) {
        mylite_execution_set_unsupported_error(database, message);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int note_secondary_index_presence(
    const struct mylite_catalog_index_descriptor *index,
    void *user_data
) {
    struct secondary_index_presence_context *context = user_data;

    if (index->kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY) {
        context->has_secondary_index = true;
    }

    return MYLITE_OK;
}

int mylite_execution_reject_check_constraint_table_alter(
    struct mylite_db *database,
    int64_t table_id,
    const char *message
) {
    struct check_constraint_presence_context context = {.has_check_constraint = false};
    int rc = mylite_catalog_for_each_check_constraint_in_table(
        database,
        table_id,
        note_check_constraint_presence,
        &context
    );

    if (rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(database, "failed to load CHECK constraint descriptors");
        return rc;
    }
    if (context.has_check_constraint) {
        mylite_execution_set_unsupported_error(database, message);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int note_check_constraint_presence(
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
    void *user_data
) {
    struct check_constraint_presence_context *context = user_data;

    (void)check_constraint;
    context->has_check_constraint = true;
    return MYLITE_OK;
}

static int append_loaded_primary_key_column(
    const struct mylite_catalog_index_column_descriptor *index_column,
    void *user_data
) {
    struct load_primary_key_column_context *context = user_data;
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    if (index_column == NULL || context == NULL || context->database == NULL) {
        return MYLITE_MISUSE;
    }
    rc = mylite_execution_find_column_index_by_id(
        context->columns,
        context->column_count,
        &column_index,
        index_column->column_id
    );
    if (rc != MYLITE_OK) {
        mylite_execution_set_runtime_error(
            context->database,
            "primary-key column descriptor is stale"
        );
        return MYLITE_ERROR;
    }
    if (context->count >= context->capacity) {
        enum { initial_primary_key_part_capacity = 4 };
        struct loaded_index_part *parts = NULL;
        size_t capacity =
            context->capacity == 0U ? initial_primary_key_part_capacity : context->capacity;

        while (capacity <= context->count) {
            if (capacity > SIZE_MAX / 2U) {
                mylite_execution_set_nomem_error(context->database);
                return MYLITE_NOMEM;
            }
            capacity *= 2U;
        }
        if (capacity > SIZE_MAX / sizeof(*parts)) {
            mylite_execution_set_nomem_error(context->database);
            return MYLITE_NOMEM;
        }
        parts = realloc(context->parts, capacity * sizeof(*parts));
        if (parts == NULL) {
            mylite_execution_set_nomem_error(context->database);
            return MYLITE_NOMEM;
        }
        context->parts = parts;
        context->capacity = capacity;
    }
    context->parts[context->count] = (struct loaded_index_part){
        .index_column = *index_column,
        .column = context->columns[column_index],
        .column_index = column_index,
    };
    ++context->count;

    return MYLITE_OK;
}

int mylite_execution_find_column_index_by_id(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    size_t *out_index,
    int64_t column_id
) {
    *out_index = 0U;
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        if (columns[column_index].column_id == column_id) {
            *out_index = column_index;
            return MYLITE_OK;
        }
    }

    return MYLITE_ERROR;
}

static int load_columns_reserve(struct load_columns_context *context, size_t required_capacity) {
    enum { initial_column_capacity = 4 };

    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t capacity = context->capacity;

    if (required_capacity <= capacity) {
        return MYLITE_OK;
    }
    if (capacity == 0U) {
        capacity = initial_column_capacity;
    }
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*columns)) {
        return MYLITE_NOMEM;
    }

    columns = realloc(context->columns, capacity * sizeof(*columns));
    if (columns == NULL) {
        return MYLITE_NOMEM;
    }

    context->columns = columns;
    context->capacity = capacity;

    return MYLITE_OK;
}
