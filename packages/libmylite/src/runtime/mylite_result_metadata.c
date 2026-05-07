#include "mylite_result_metadata.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int reserve_columns(struct mylite_result_metadata *metadata, size_t required_capacity);
static int materialize_column(
    struct mylite_result_column *column,
    const struct mylite_result_column_descriptor *descriptor
);
static void deinit_column(struct mylite_result_column *column);
static char *duplicate_text(const char *text);

void mylite_result_metadata_init(struct mylite_result_metadata *metadata) {
    if (metadata == NULL) {
        return;
    }

    metadata->columns = NULL;
    metadata->column_count = 0U;
    metadata->column_capacity = 0U;
}

void mylite_result_metadata_deinit(struct mylite_result_metadata *metadata) {
    size_t index = 0U;

    if (metadata == NULL) {
        return;
    }

    for (index = 0U; index < metadata->column_count; ++index) {
        deinit_column(&metadata->columns[index]);
    }
    free(metadata->columns);
    metadata->columns = NULL;
    metadata->column_count = 0U;
    metadata->column_capacity = 0U;
}

int mylite_result_metadata_append(
    struct mylite_result_metadata *metadata,
    const struct mylite_result_column_descriptor *descriptor
) {
    struct mylite_result_column column = {0};
    int rc = MYLITE_OK;

    if (metadata == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    if (metadata->column_count == SIZE_MAX) {
        return MYLITE_NOMEM;
    }

    rc = materialize_column(&column, descriptor);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = reserve_columns(metadata, metadata->column_count + 1U);
    if (rc != MYLITE_OK) {
        deinit_column(&column);
        return rc;
    }

    metadata->columns[metadata->column_count] = column;
    ++metadata->column_count;

    return MYLITE_OK;
}

size_t mylite_result_metadata_column_count(const struct mylite_result_metadata *metadata) {
    if (metadata == NULL) {
        return 0U;
    }

    return metadata->column_count;
}

const struct mylite_result_column *mylite_result_metadata_column_at(
    const struct mylite_result_metadata *metadata,
    size_t index
) {
    if (metadata == NULL || index >= metadata->column_count) {
        return NULL;
    }

    return &metadata->columns[index];
}

static int reserve_columns(struct mylite_result_metadata *metadata, size_t required_capacity) {
    enum { initial_column_capacity = 4 };

    struct mylite_result_column *columns = NULL;
    size_t capacity = metadata->column_capacity;

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

    columns = realloc(metadata->columns, capacity * sizeof(*columns));
    if (columns == NULL) {
        return MYLITE_NOMEM;
    }

    metadata->columns = columns;
    metadata->column_capacity = capacity;

    return MYLITE_OK;
}

static int materialize_column(
    struct mylite_result_column *column,
    const struct mylite_result_column_descriptor *descriptor
) {
    column->label = duplicate_text(descriptor->label);
    column->schema_name = duplicate_text(descriptor->schema_name);
    column->table_name = duplicate_text(descriptor->table_name);
    column->origin_schema_name = duplicate_text(descriptor->origin_schema_name);
    column->origin_table_name = duplicate_text(descriptor->origin_table_name);
    column->origin_column_name = duplicate_text(descriptor->origin_column_name);
    column->logical_type = descriptor->logical_type;
    column->flags = descriptor->flags;
    column->charset_id = descriptor->charset_id;
    column->collation_id = descriptor->collation_id;
    column->display_length = descriptor->display_length;
    column->decimals = descriptor->decimals;
    column->nullable = descriptor->nullable;

    if (column->label == NULL || column->schema_name == NULL || column->table_name == NULL ||
        column->origin_schema_name == NULL || column->origin_table_name == NULL ||
        column->origin_column_name == NULL) {
        deinit_column(column);
        return MYLITE_NOMEM;
    }

    return MYLITE_OK;
}

static void deinit_column(struct mylite_result_column *column) {
    if (column == NULL) {
        return;
    }

    free(column->label);
    free(column->schema_name);
    free(column->table_name);
    free(column->origin_schema_name);
    free(column->origin_table_name);
    free(column->origin_column_name);
    *column = (struct mylite_result_column){0};
}

static char *duplicate_text(const char *text) {
    const char *source = text == NULL ? "" : text;
    size_t length = strlen(source);
    size_t size = 0U;
    char *copy = NULL;

    if (length == SIZE_MAX) {
        return NULL;
    }
    size = length + 1U;
    copy = malloc(size);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, source, size);

    return copy;
}
