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
static struct mylite_temporary_catalog_table *find_table_by_id_mutable(
    struct mylite_temporary_catalog *catalog,
    int64_t table_id
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
static int copy_column_with_owned_strings(
    const struct mylite_catalog_column_descriptor *source,
    struct mylite_catalog_column_descriptor *out_column
);
static int duplicate_column_string(const char *source, const char **out_text);

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

int mylite_temporary_catalog_update_table_auto_increment_next(
    struct mylite_temporary_catalog *catalog,
    int64_t table_id, // NOLINT(bugprone-easily-swappable-parameters): mirror durable catalog API.
    int64_t auto_increment_next
) {
    if (catalog == NULL || table_id >= 0) {
        return MYLITE_MISUSE;
    }
    if (auto_increment_next <= 0) {
        return MYLITE_ERROR;
    }

    for (size_t index = 0U; index < catalog->table_count; ++index) {
        if (catalog->tables[index].table.table_id == table_id) {
            catalog->tables[index].table.auto_increment_next = auto_increment_next;
            return MYLITE_OK;
        }
    }

    return MYLITE_ERROR;
}

int mylite_temporary_catalog_update_table_comment(
    struct mylite_temporary_catalog *catalog,
    int64_t table_id,
    const char *comment
) {
    if (catalog == NULL || table_id >= 0 || comment == NULL) {
        return MYLITE_MISUSE;
    }
    if (strlen(comment) >= MYLITE_CATALOG_TABLE_COMMENT_CAPACITY) {
        return MYLITE_ERROR;
    }

    for (size_t index = 0U; index < catalog->table_count; ++index) {
        if (catalog->tables[index].table.table_id == table_id) {
            snprintf(
                catalog->tables[index].table.comment,
                sizeof(catalog->tables[index].table.comment),
                "%s",
                comment
            );
            return MYLITE_OK;
        }
    }

    return MYLITE_ERROR;
}

int mylite_temporary_catalog_append_column(
    struct mylite_temporary_catalog *catalog,
    int64_t table_id,
    const struct mylite_catalog_column_descriptor *column,
    size_t target_column_index
) {
    struct mylite_temporary_catalog_table *table = NULL;
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct mylite_catalog_column_descriptor owned_column = {0};

    if (catalog == NULL || column == NULL || table_id >= 0 || column->column_id >= 0 ||
        column->table_id != table_id) {
        return MYLITE_MISUSE;
    }

    table = find_table_by_id_mutable(catalog, table_id);
    if (table == NULL || target_column_index > table->column_count) {
        return MYLITE_ERROR;
    }
    for (size_t column_index = 0U; column_index < table->column_count; ++column_index) {
        if (table->columns[column_index].column_id == column->column_id ||
            strcmp(table->columns[column_index].name, column->name) == 0) {
            return MYLITE_ERROR;
        }
    }
    if (table->column_count == SIZE_MAX ||
        table->column_count + 1U > SIZE_MAX / sizeof(*table->columns)) {
        return MYLITE_NOMEM;
    }
    int rc = copy_column_with_owned_strings(column, &owned_column);

    if (rc != MYLITE_OK) {
        return rc;
    }

    columns = realloc(table->columns, (table->column_count + 1U) * sizeof(*table->columns));
    if (columns == NULL) {
        mylite_temporary_catalog_column_strings_deinit(&owned_column);
        return MYLITE_NOMEM;
    }
    table->columns = columns;
    for (size_t index = table->column_count; index > target_column_index; --index) {
        table->columns[index] = table->columns[index - 1U];
    }

    table->columns[target_column_index] = owned_column;
    ++table->column_count;
    for (size_t index = 0U; index < table->column_count; ++index) {
        table->columns[index].ordinal_position = (int64_t)index + 1;
    }
    return MYLITE_OK;
}

int mylite_temporary_catalog_replace_column(
    struct mylite_temporary_catalog *catalog,
    int64_t table_id,
    int64_t column_id,
    const struct mylite_catalog_column_descriptor *column
) {
    struct mylite_temporary_catalog_table *table = NULL;
    struct mylite_catalog_column_descriptor owned_column = {0};
    size_t column_position = 0U;
    bool found = false;

    if (catalog == NULL || table_id >= 0 || column_id >= 0 || column == NULL ||
        column->table_id != table_id || column->column_id != column_id) {
        return MYLITE_MISUSE;
    }

    table = find_table_by_id_mutable(catalog, table_id);
    if (table == NULL) {
        return MYLITE_ERROR;
    }
    for (; column_position < table->column_count; ++column_position) {
        const struct mylite_catalog_column_descriptor *current = &table->columns[column_position];

        if (current->column_id == column_id) {
            found = true;
            continue;
        }
        if (strcmp(current->name, column->name) == 0) {
            return MYLITE_ERROR;
        }
    }
    if (!found) {
        return MYLITE_ERROR;
    }
    int rc = copy_column_with_owned_strings(column, &owned_column);

    if (rc != MYLITE_OK) {
        return rc;
    }

    for (column_position = 0U; column_position < table->column_count; ++column_position) {
        if (table->columns[column_position].column_id == column_id) {
            mylite_temporary_catalog_column_strings_deinit(&table->columns[column_position]);
            table->columns[column_position] = owned_column;
            table->columns[column_position].ordinal_position = (int64_t)column_position + 1;
            return MYLITE_OK;
        }
    }

    mylite_temporary_catalog_column_strings_deinit(&owned_column);
    return MYLITE_ERROR;
}

int mylite_temporary_catalog_replace_columns(
    struct mylite_temporary_catalog *catalog,
    int64_t table_id,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
) {
    struct mylite_temporary_catalog_table *table = NULL;
    struct mylite_catalog_column_descriptor *replacement = NULL;

    if (catalog == NULL || table_id >= 0 || columns == NULL || column_count == 0U) {
        return MYLITE_MISUSE;
    }
    if (column_count > SIZE_MAX / sizeof(*replacement)) {
        return MYLITE_NOMEM;
    }

    table = find_table_by_id_mutable(catalog, table_id);
    if (table == NULL) {
        return MYLITE_ERROR;
    }
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        if (columns[column_index].table_id != table_id || columns[column_index].column_id >= 0) {
            return MYLITE_MISUSE;
        }
        for (size_t prior_index = 0U; prior_index < column_index; ++prior_index) {
            if (columns[prior_index].column_id == columns[column_index].column_id ||
                strcmp(columns[prior_index].name, columns[column_index].name) == 0) {
                return MYLITE_ERROR;
            }
        }
    }

    replacement = calloc(column_count, sizeof(*replacement));
    if (replacement == NULL) {
        return MYLITE_NOMEM;
    }
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        int rc = copy_column_with_owned_strings(&columns[column_index], &replacement[column_index]);

        if (rc != MYLITE_OK) {
            for (size_t owned_index = 0U; owned_index < column_index; ++owned_index) {
                mylite_temporary_catalog_column_strings_deinit(&replacement[owned_index]);
            }
            free(replacement);
            return rc;
        }
        replacement[column_index].ordinal_position = (int64_t)column_index + 1;
    }

    for (size_t column_index = 0U; column_index < table->column_count; ++column_index) {
        mylite_temporary_catalog_column_strings_deinit(&table->columns[column_index]);
    }
    free(table->columns);
    table->columns = replacement;
    table->column_count = column_count;
    return MYLITE_OK;
}

int mylite_temporary_catalog_remove_column_by_id(
    struct mylite_temporary_catalog *catalog,
    int64_t table_id,
    int64_t column_id
) {
    struct mylite_temporary_catalog_table *table = NULL;
    size_t column_position = 0U;
    bool found = false;

    if (catalog == NULL || table_id >= 0 || column_id >= 0) {
        return MYLITE_MISUSE;
    }

    table = find_table_by_id_mutable(catalog, table_id);
    if (table == NULL) {
        return MYLITE_ERROR;
    }
    for (; column_position < table->column_count; ++column_position) {
        if (table->columns[column_position].column_id == column_id) {
            found = true;
            break;
        }
    }
    if (!found) {
        return MYLITE_ERROR;
    }

    mylite_temporary_catalog_column_strings_deinit(&table->columns[column_position]);
    for (size_t index = column_position + 1U; index < table->column_count; ++index) {
        table->columns[index - 1U] = table->columns[index];
    }
    --table->column_count;
    if (table->columns != NULL) {
        table->columns[table->column_count] = (struct mylite_catalog_column_descriptor){0};
    }
    for (size_t index = 0U; index < table->column_count; ++index) {
        table->columns[index].ordinal_position = (int64_t)index + 1;
    }
    return MYLITE_OK;
}

int mylite_temporary_catalog_append_index(
    struct mylite_temporary_catalog *catalog,
    int64_t table_id,
    const struct mylite_catalog_index_descriptor *index,
    const struct mylite_catalog_index_column_descriptor *index_columns,
    size_t index_column_count
) {
    struct mylite_temporary_catalog_table *table = NULL;
    struct mylite_catalog_index_descriptor *indexes = NULL;
    struct mylite_catalog_index_column_descriptor *columns = NULL;

    if (catalog == NULL || index == NULL || table_id >= 0 || index->index_id >= 0 ||
        index->table_id != table_id || (index_column_count > 0U && index_columns == NULL)) {
        return MYLITE_MISUSE;
    }
    for (size_t part_index = 0U; part_index < index_column_count; ++part_index) {
        if (index_columns[part_index].index_column_id >= 0 ||
            index_columns[part_index].index_id != index->index_id ||
            index_columns[part_index].table_id != table_id) {
            return MYLITE_MISUSE;
        }
    }

    table = find_table_by_id_mutable(catalog, table_id);
    if (table == NULL) {
        return MYLITE_ERROR;
    }
    for (size_t index_index = 0U; index_index < table->index_count; ++index_index) {
        if (table->indexes[index_index].index_id == index->index_id ||
            strcmp(table->indexes[index_index].name, index->name) == 0) {
            return MYLITE_ERROR;
        }
    }
    if (table->index_count == SIZE_MAX ||
        table->index_count + 1U > SIZE_MAX / sizeof(*table->indexes) ||
        index_column_count > SIZE_MAX - table->index_column_count ||
        table->index_column_count + index_column_count > SIZE_MAX / sizeof(*table->index_columns)) {
        return MYLITE_NOMEM;
    }

    indexes = realloc(table->indexes, (table->index_count + 1U) * sizeof(*table->indexes));
    if (indexes == NULL) {
        return MYLITE_NOMEM;
    }
    table->indexes = indexes;
    if (index_column_count > 0U) {
        columns = realloc(
            table->index_columns,
            (table->index_column_count + index_column_count) * sizeof(*table->index_columns)
        );
        if (columns == NULL) {
            return MYLITE_NOMEM;
        }
        table->index_columns = columns;
    }

    table->indexes[table->index_count] = *index;
    ++table->index_count;
    for (size_t part_index = 0U; part_index < index_column_count; ++part_index) {
        table->index_columns[table->index_column_count + part_index] = index_columns[part_index];
    }
    table->index_column_count += index_column_count;
    return MYLITE_OK;
}

int mylite_temporary_catalog_remove_index_by_id(
    struct mylite_temporary_catalog *catalog,
    int64_t table_id,
    int64_t index_id
) {
    struct mylite_temporary_catalog_table *table = NULL;
    size_t index_position = 0U;
    bool found = false;
    size_t write_position = 0U;

    if (catalog == NULL || table_id >= 0 || index_id >= 0) {
        return MYLITE_MISUSE;
    }

    table = find_table_by_id_mutable(catalog, table_id);
    if (table == NULL) {
        return MYLITE_ERROR;
    }
    for (; index_position < table->index_count; ++index_position) {
        if (table->indexes[index_position].index_id == index_id) {
            found = true;
            break;
        }
    }
    if (!found) {
        return MYLITE_ERROR;
    }

    for (size_t index = index_position + 1U; index < table->index_count; ++index) {
        table->indexes[index - 1U] = table->indexes[index];
    }
    --table->index_count;
    if (table->indexes != NULL) {
        table->indexes[table->index_count] = (struct mylite_catalog_index_descriptor){0};
    }

    for (size_t read_position = 0U; read_position < table->index_column_count; ++read_position) {
        if (table->index_columns[read_position].index_id == index_id) {
            continue;
        }
        if (write_position != read_position) {
            table->index_columns[write_position] = table->index_columns[read_position];
        }
        ++write_position;
    }
    for (size_t clear_position = write_position; clear_position < table->index_column_count;
         ++clear_position) {
        table->index_columns[clear_position] = (struct mylite_catalog_index_column_descriptor){0};
    }
    table->index_column_count = write_position;
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

    for (size_t index = 0U; index < table->column_count; ++index) {
        mylite_temporary_catalog_column_strings_deinit(&table->columns[index]);
    }
    free(table->columns);
    free(table->indexes);
    free(table->index_columns);
    *table = (struct mylite_temporary_catalog_table){0};
}

int mylite_temporary_catalog_column_strings_own(struct mylite_catalog_column_descriptor *column) {
    struct mylite_catalog_column_descriptor owned = {0};
    int rc = MYLITE_OK;

    if (column == NULL) {
        return MYLITE_MISUSE;
    }
    rc = copy_column_with_owned_strings(column, &owned);
    if (rc != MYLITE_OK) {
        return rc;
    }
    column->default_text = owned.default_text;
    column->comment = owned.comment;
    column->generation_expression = owned.generation_expression;
    column->sqlite_generation_expression = owned.sqlite_generation_expression;
    return MYLITE_OK;
}

void mylite_temporary_catalog_column_strings_deinit(struct mylite_catalog_column_descriptor *column
) {
    if (column == NULL) {
        return;
    }

    free((void *)column->default_text);
    free((void *)column->comment);
    free((void *)column->generation_expression);
    free((void *)column->sqlite_generation_expression);
    column->default_text = NULL;
    column->comment = NULL;
    column->generation_expression = NULL;
    column->sqlite_generation_expression = NULL;
}

static int copy_column_with_owned_strings(
    const struct mylite_catalog_column_descriptor *source,
    struct mylite_catalog_column_descriptor *out_column
) {
    int rc = MYLITE_OK;

    *out_column = *source;
    out_column->default_text = NULL;
    out_column->comment = NULL;
    out_column->generation_expression = NULL;
    out_column->sqlite_generation_expression = NULL;
    rc = duplicate_column_string(source->default_text, &out_column->default_text);
    if (rc == MYLITE_OK) {
        rc = duplicate_column_string(source->comment, &out_column->comment);
    }
    if (rc == MYLITE_OK) {
        rc = duplicate_column_string(
            source->generation_expression,
            &out_column->generation_expression
        );
    }
    if (rc == MYLITE_OK) {
        rc = duplicate_column_string(
            source->sqlite_generation_expression,
            &out_column->sqlite_generation_expression
        );
    }
    if (rc != MYLITE_OK) {
        mylite_temporary_catalog_column_strings_deinit(out_column);
    }
    return rc;
}

static int duplicate_column_string(const char *source, const char **out_text) {
    char *copy = NULL;
    size_t length = 0U;

    if (source == NULL) {
        source = "";
    }
    length = strlen(source);
    if (length == SIZE_MAX) {
        return MYLITE_NOMEM;
    }
    copy = (char *)malloc(length + 1U);
    if (copy == NULL) {
        return MYLITE_NOMEM;
    }
    memcpy(copy, source, length + 1U);
    *out_text = copy;
    return MYLITE_OK;
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

static struct mylite_temporary_catalog_table *find_table_by_id_mutable(
    struct mylite_temporary_catalog *catalog,
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
