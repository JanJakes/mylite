#include "mylite_temporary_catalog.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int allocate_negative_id(int64_t *next_id, int64_t *out_id);
static int reserve_temporary_tables(
    struct mylite_temporary_catalog *catalog,
    size_t required_capacity
);
static const struct mylite_temporary_catalog_table *find_table_by_id(
    const struct mylite_temporary_catalog *catalog,
    int64_t table_id
);
static const struct mylite_temporary_catalog_table *find_table_by_schema_name(
    const struct mylite_temporary_catalog *catalog,
    const char *schema_name,
    const char *table_name
);
static int build_temporary_table_name(
    uint64_t physical_id,
    char *destination,
    size_t destination_size
);
static int build_temporary_index_name(
    uint64_t physical_id,
    char *destination,
    size_t destination_size
);

void mylite_temporary_catalog_init(struct mylite_temporary_catalog *catalog) {
    if (catalog == NULL) {
        return;
    }

    *catalog = (struct mylite_temporary_catalog){
        .initialized = true,
        .next_table_id = -1,
        .next_column_id = -1,
        .next_index_id = -1,
        .next_index_column_id = -1,
        .next_physical_table_id = 1U,
        .next_physical_index_id = 1U,
    };
}

void mylite_temporary_catalog_deinit(struct mylite_temporary_catalog *catalog) {
    if (catalog == NULL) {
        return;
    }

    for (size_t index = 0U; index < catalog->table_count; ++index) {
        mylite_temporary_catalog_table_deinit(&catalog->tables[index]);
    }
    free(catalog->tables);
    *catalog = (struct mylite_temporary_catalog){.initialized = false};
}

int mylite_temporary_catalog_allocate_table_identity(
    struct mylite_temporary_catalog *catalog,
    int64_t *out_table_id,
    char *physical_name,
    size_t physical_name_size
) {
    int rc = MYLITE_OK;

    if (catalog == NULL || out_table_id == NULL || physical_name == NULL) {
        return MYLITE_MISUSE;
    }
    if (catalog->next_physical_table_id == UINT64_MAX) {
        return MYLITE_ERROR;
    }

    rc = allocate_negative_id(&catalog->next_table_id, out_table_id);
    if (rc == MYLITE_OK) {
        rc = build_temporary_table_name(
            catalog->next_physical_table_id,
            physical_name,
            physical_name_size
        );
    }
    if (rc == MYLITE_OK) {
        ++catalog->next_physical_table_id;
    }
    return rc;
}

int mylite_temporary_catalog_allocate_column_id(
    struct mylite_temporary_catalog *catalog,
    int64_t *out_column_id
) {
    if (catalog == NULL || out_column_id == NULL) {
        return MYLITE_MISUSE;
    }
    return allocate_negative_id(&catalog->next_column_id, out_column_id);
}

int mylite_temporary_catalog_allocate_index_identity(
    struct mylite_temporary_catalog *catalog,
    int64_t *out_index_id,
    char *physical_name,
    size_t physical_name_size
) {
    int rc = MYLITE_OK;

    if (catalog == NULL || out_index_id == NULL || physical_name == NULL) {
        return MYLITE_MISUSE;
    }
    if (catalog->next_physical_index_id == UINT64_MAX) {
        return MYLITE_ERROR;
    }

    rc = allocate_negative_id(&catalog->next_index_id, out_index_id);
    if (rc == MYLITE_OK) {
        rc = build_temporary_index_name(
            catalog->next_physical_index_id,
            physical_name,
            physical_name_size
        );
    }
    if (rc == MYLITE_OK) {
        ++catalog->next_physical_index_id;
    }
    return rc;
}

int mylite_temporary_catalog_allocate_index_column_id(
    struct mylite_temporary_catalog *catalog,
    int64_t *out_index_column_id
) {
    if (catalog == NULL || out_index_column_id == NULL) {
        return MYLITE_MISUSE;
    }
    return allocate_negative_id(&catalog->next_index_column_id, out_index_column_id);
}

int mylite_temporary_catalog_append_table(
    struct mylite_temporary_catalog *catalog,
    struct mylite_temporary_catalog_table *table
) {
    int rc = MYLITE_OK;

    if (catalog == NULL || table == NULL || table->table.table_id >= 0 ||
        table->schema_name[0] == '\0' || table->table.name[0] == '\0') {
        return MYLITE_MISUSE;
    }
    if (find_table_by_schema_name(catalog, table->schema_name, table->table.name) != NULL) {
        return MYLITE_ERROR;
    }

    rc = reserve_temporary_tables(catalog, catalog->table_count + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }

    catalog->tables[catalog->table_count] = *table;
    ++catalog->table_count;
    *table = (struct mylite_temporary_catalog_table){0};
    return MYLITE_OK;
}

int mylite_temporary_catalog_remove_table_by_id(
    struct mylite_temporary_catalog *catalog,
    int64_t table_id
) {
    struct mylite_temporary_catalog_table *table = NULL;
    size_t table_index = 0U;

    if (catalog == NULL || table_id >= 0) {
        return MYLITE_MISUSE;
    }
    for (; table_index < catalog->table_count; ++table_index) {
        if (catalog->tables[table_index].table.table_id == table_id) {
            table = &catalog->tables[table_index];
            break;
        }
    }
    if (table == NULL) {
        return MYLITE_ERROR;
    }

    mylite_temporary_catalog_table_deinit(table);
    for (size_t index = table_index + 1U; index < catalog->table_count; ++index) {
        catalog->tables[index - 1U] = catalog->tables[index];
    }
    --catalog->table_count;
    if (catalog->table_count < catalog->table_capacity) {
        catalog->tables[catalog->table_count] = (struct mylite_temporary_catalog_table){0};
    }
    return MYLITE_OK;
}

int mylite_temporary_catalog_try_read_table_by_name(
    const struct mylite_temporary_catalog *catalog,
    const char *schema_name,
    const char *table_name,
    struct mylite_catalog_table_descriptor *out_table,
    bool *out_found
) {
    const struct mylite_temporary_catalog_table *table = NULL;

    if (out_table == NULL || out_found == NULL) {
        return MYLITE_MISUSE;
    }
    *out_table = (struct mylite_catalog_table_descriptor){0};
    *out_found = false;
    if (catalog == NULL || schema_name == NULL || table_name == NULL) {
        return MYLITE_MISUSE;
    }

    table = find_table_by_schema_name(catalog, schema_name, table_name);
    if (table == NULL) {
        return MYLITE_OK;
    }

    *out_table = table->table;
    *out_found = true;
    return MYLITE_OK;
}

int mylite_temporary_catalog_for_each_column_in_table(
    const struct mylite_temporary_catalog *catalog,
    int64_t table_id,
    mylite_catalog_column_callback callback,
    void *user_data
) {
    const struct mylite_temporary_catalog_table *table = NULL;

    if (callback == NULL || table_id >= 0) {
        return MYLITE_MISUSE;
    }
    table = find_table_by_id(catalog, table_id);
    if (table == NULL) {
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; index < table->column_count; ++index) {
        int rc = callback(&table->columns[index], user_data);

        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

int mylite_temporary_catalog_for_each_index_in_table(
    const struct mylite_temporary_catalog *catalog,
    int64_t table_id,
    mylite_catalog_index_callback callback,
    void *user_data
) {
    const struct mylite_temporary_catalog_table *table = NULL;

    if (callback == NULL || table_id >= 0) {
        return MYLITE_MISUSE;
    }
    table = find_table_by_id(catalog, table_id);
    if (table == NULL) {
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; index < table->index_count; ++index) {
        int rc = callback(&table->indexes[index], user_data);

        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

int mylite_temporary_catalog_for_each_index_column_in_index(
    const struct mylite_temporary_catalog *catalog,
    int64_t index_id,
    mylite_catalog_index_column_callback callback,
    void *user_data
) {
    if (callback == NULL || index_id >= 0) {
        return MYLITE_MISUSE;
    }
    for (size_t table_index = 0U; catalog != NULL && table_index < catalog->table_count;
         ++table_index) {
        const struct mylite_temporary_catalog_table *table = &catalog->tables[table_index];

        for (size_t index = 0U; index < table->index_column_count; ++index) {
            int rc = MYLITE_OK;

            if (table->index_columns[index].index_id != index_id) {
                continue;
            }
            rc = callback(&table->index_columns[index], user_data);
            if (rc != MYLITE_OK) {
                return rc;
            }
        }
    }
    return MYLITE_OK;
}

int mylite_temporary_catalog_try_read_primary_index_by_table_id(
    const struct mylite_temporary_catalog *catalog,
    int64_t table_id,
    struct mylite_catalog_index_descriptor *out_index,
    bool *out_found
) {
    const struct mylite_temporary_catalog_table *table = NULL;

    if (out_index == NULL || out_found == NULL || table_id >= 0) {
        return MYLITE_MISUSE;
    }
    *out_index = (struct mylite_catalog_index_descriptor){0};
    *out_found = false;
    table = find_table_by_id(catalog, table_id);
    if (table == NULL) {
        return MYLITE_ERROR;
    }

    for (size_t index = 0U; index < table->index_count; ++index) {
        if (table->indexes[index].kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
            *out_index = table->indexes[index];
            *out_found = true;
            return MYLITE_OK;
        }
    }
    return MYLITE_OK;
}

void mylite_temporary_catalog_table_deinit(struct mylite_temporary_catalog_table *table) {
    if (table == NULL) {
        return;
    }

    free(table->columns);
    free(table->indexes);
    free(table->index_columns);
    *table = (struct mylite_temporary_catalog_table){0};
}

static int allocate_negative_id(int64_t *next_id, int64_t *out_id) {
    if (next_id == NULL || out_id == NULL) {
        return MYLITE_MISUSE;
    }
    if (*next_id >= 0 || *next_id == INT64_MIN) {
        return MYLITE_ERROR;
    }

    *out_id = *next_id;
    --*next_id;
    return MYLITE_OK;
}

static int reserve_temporary_tables(
    struct mylite_temporary_catalog *catalog,
    size_t required_capacity
) {
    enum { initial_table_capacity = 4 };

    struct mylite_temporary_catalog_table *tables = NULL;
    size_t capacity = 0U;

    if (required_capacity <= catalog->table_capacity) {
        return MYLITE_OK;
    }
    capacity = catalog->table_capacity == 0U ? initial_table_capacity : catalog->table_capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*tables)) {
        return MYLITE_NOMEM;
    }

    tables = realloc(catalog->tables, capacity * sizeof(*tables));
    if (tables == NULL) {
        return MYLITE_NOMEM;
    }
    for (size_t index = catalog->table_capacity; index < capacity; ++index) {
        tables[index] = (struct mylite_temporary_catalog_table){0};
    }

    catalog->tables = tables;
    catalog->table_capacity = capacity;
    return MYLITE_OK;
}

static const struct mylite_temporary_catalog_table *find_table_by_id(
    const struct mylite_temporary_catalog *catalog,
    int64_t table_id
) {
    if (catalog == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < catalog->table_count; ++index) {
        if (catalog->tables[index].table.table_id == table_id) {
            return &catalog->tables[index];
        }
    }
    return NULL;
}

static const struct mylite_temporary_catalog_table *find_table_by_schema_name(
    const struct mylite_temporary_catalog *catalog,
    const char *schema_name,
    const char *table_name
) {
    if (catalog == NULL || schema_name == NULL || table_name == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < catalog->table_count; ++index) {
        const struct mylite_temporary_catalog_table *table = &catalog->tables[index];

        if (strcmp(table->schema_name, schema_name) == 0 &&
            strcmp(table->table.name, table_name) == 0) {
            return table;
        }
    }
    return NULL;
}

static int build_temporary_table_name(
    uint64_t physical_id,
    char *destination,
    size_t destination_size
) {
    int written =
        snprintf(destination, destination_size, "_mylite_temp_table_%" PRIu64, physical_id);

    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int build_temporary_index_name(
    uint64_t physical_id,
    char *destination,
    size_t destination_size
) {
    int written =
        snprintf(destination, destination_size, "_mylite_temp_index_%" PRIu64, physical_id);

    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}
