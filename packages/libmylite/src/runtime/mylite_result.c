#include "mylite_result.h"
#ifdef MYLITE_ENABLE_PROFILING
#  include "mylite_profile_internal.h"
#endif

#include <stdlib.h>
#include <string.h>

enum {
    text_row_stack_cell_capacity = 32,
};

static int reserve_columns(mylite_result *result, size_t required_capacity);
static int reserve_rows(mylite_result *result, size_t required_capacity);
static int duplicate_bytes(const void *bytes, size_t size, char **out_copy);
static char *duplicate_text(const char *text);
static void free_values(char **values, size_t value_count);

int mylite_result_create(mylite_result **out_result) {
    mylite_result *result = NULL;

    if (out_result == NULL) {
        return MYLITE_MISUSE;
    }

    *out_result = NULL;
    result = calloc(1U, sizeof(*result));
    if (result == NULL) {
        return MYLITE_NOMEM;
    }

    mylite_result_metadata_init(&result->metadata);
    *out_result = result;

    return MYLITE_OK;
}

void mylite_result_free(mylite_result *result) {
    if (result == NULL) {
        return;
    }

    free_values(result->column_names, result->column_count);
    mylite_result_metadata_deinit(&result->metadata);
    free_values(result->values, result->row_count * result->column_count);
    free(result->value_sizes);
    free(result->info);
    free(result);
}

int mylite_result_append_column(mylite_result *result, const char *name) {
    const struct mylite_result_column_descriptor descriptor = {
        .label = name,
        .schema_name = "",
        .table_name = "",
        .origin_schema_name = "",
        .origin_table_name = "",
        .origin_column_name = "",
        .logical_type = MYLITE_RESULT_LOGICAL_TYPE_UNKNOWN,
        .flags = 0U,
        .charset_id = 0U,
        .collation_id = 0U,
        .display_length = 0U,
        .decimals = 0U,
        .nullable = true,
    };

    return mylite_result_append_column_descriptor(result, &descriptor);
}

int mylite_result_append_column_descriptor(
    mylite_result *result,
    const struct mylite_result_column_descriptor *descriptor
) {
    char *owned_name = NULL;
    int rc = MYLITE_OK;

    if (result == NULL || descriptor == NULL || descriptor->label == NULL ||
        result->row_count != 0U) {
        return MYLITE_MISUSE;
    }

    rc = reserve_columns(result, result->column_count + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }

    owned_name = duplicate_text(descriptor->label);
    if (owned_name == NULL) {
        return MYLITE_NOMEM;
    }

    rc = mylite_result_metadata_append(&result->metadata, descriptor);
    if (rc != MYLITE_OK) {
        free(owned_name);
        return rc;
    }

    result->column_names[result->column_count] = owned_name;
    ++result->column_count;

    return MYLITE_OK;
}

int mylite_result_append_bytes_row(mylite_result *result, const struct mylite_result_cell *values) {
    size_t column_count = 0U;
    size_t value_offset = 0U;
#ifdef MYLITE_ENABLE_PROFILING
    size_t value_bytes = 0U;
    uint64_t profile_started_ns = 0U;
#endif
    int rc = MYLITE_OK;

    if (result == NULL || values == NULL || result->column_count == 0U) {
        return MYLITE_MISUSE;
    }

#ifdef MYLITE_ENABLE_PROFILING
    profile_started_ns = mylite_profile_now_ns();
#endif
    column_count = result->column_count;
    rc = reserve_rows(result, result->row_count + 1U);
    if (rc != MYLITE_OK) {
#ifdef MYLITE_ENABLE_PROFILING
        mylite_profile_record_result_buffer(profile_started_ns, false, 0U);
#endif
        return rc;
    }

    value_offset = result->row_count * column_count;
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        const struct mylite_result_cell *cell = &values[column_index];

        if (cell->is_null) {
            result->values[value_offset + column_index] = NULL;
            result->value_sizes[value_offset + column_index] = 0U;
            continue;
        }
        rc = duplicate_bytes(cell->bytes, cell->size, &result->values[value_offset + column_index]);
        if (rc != MYLITE_OK) {
            for (size_t rollback_index = 0U; rollback_index < column_index; ++rollback_index) {
                free(result->values[value_offset + rollback_index]);
                result->values[value_offset + rollback_index] = NULL;
                result->value_sizes[value_offset + rollback_index] = 0U;
            }
#ifdef MYLITE_ENABLE_PROFILING
            mylite_profile_record_result_buffer(profile_started_ns, false, 0U);
#endif
            return rc;
        }
        result->value_sizes[value_offset + column_index] = cell->size;
#ifdef MYLITE_ENABLE_PROFILING
        value_bytes += cell->size;
#endif
    }
    ++result->row_count;
#ifdef MYLITE_ENABLE_PROFILING
    mylite_profile_record_result_buffer(profile_started_ns, true, value_bytes);
#endif

    return MYLITE_OK;
}

int mylite_result_append_text_row(mylite_result *result, const char *const *values) {
    struct mylite_result_cell stack_cells[text_row_stack_cell_capacity];
    struct mylite_result_cell *cells = stack_cells;
    int rc = MYLITE_OK;

    if (result == NULL || values == NULL || result->column_count == 0U) {
        return MYLITE_MISUSE;
    }

    if (result->column_count > text_row_stack_cell_capacity) {
        if (result->column_count > SIZE_MAX / sizeof(*cells)) {
            return MYLITE_NOMEM;
        }
        cells = (struct mylite_result_cell *)malloc(result->column_count * sizeof(*cells));
        if (cells == NULL) {
            return MYLITE_NOMEM;
        }
    }
    for (size_t column_index = 0U; column_index < result->column_count; ++column_index) {
        if (values[column_index] == NULL) {
            cells[column_index] = (struct mylite_result_cell){.is_null = true};
        } else {
            cells[column_index] = (struct mylite_result_cell){
                .bytes = values[column_index],
                .size = strlen(values[column_index]),
                .is_null = false,
            };
        }
    }
    rc = mylite_result_append_bytes_row(result, cells);
    if (cells != stack_cells) {
        free(cells);
    }

    return rc;
}

void mylite_result_truncate_rows(mylite_result *result, size_t row_count) {
    size_t first_value = 0U;
    size_t value_count = 0U;

    if (result == NULL || row_count >= result->row_count) {
        return;
    }
    first_value = row_count * result->column_count;
    value_count = (result->row_count - row_count) * result->column_count;
    for (size_t index = 0U; index < value_count; ++index) {
        free(result->values[first_value + index]);
        result->values[first_value + index] = NULL;
    }
    if (result->value_sizes != NULL) {
        memset(&result->value_sizes[first_value], 0, value_count * sizeof(*result->value_sizes));
    }
    result->row_count = row_count;
}

void mylite_result_set_affected_rows(mylite_result *result, int64_t affected_rows) {
    if (result == NULL) {
        return;
    }

    result->affected_rows = affected_rows;
}

int mylite_result_set_info(mylite_result *result, const char *info) {
    char *owned_info = NULL;

    if (result == NULL) {
        return MYLITE_MISUSE;
    }
    if (info != NULL) {
        owned_info = duplicate_text(info);
        if (owned_info == NULL) {
            return MYLITE_NOMEM;
        }
    }

    free(result->info);
    result->info = owned_info;
    return MYLITE_OK;
}

void mylite_result_set_insert_id(mylite_result *result, uint64_t insert_id) {
    if (result == NULL) {
        return;
    }

    result->insert_id = insert_id;
}

void mylite_result_set_warning_count(mylite_result *result, size_t warning_count) {
    if (result == NULL) {
        return;
    }

    result->warning_count = warning_count;
}

void mylite_result_set_found_row_count(mylite_result *result, uint64_t found_row_count) {
    if (result == NULL) {
        return;
    }

    result->has_found_row_count = true;
    result->found_row_count = found_row_count;
}

bool mylite_result_has_found_row_count(const mylite_result *result) {
    if (result == NULL) {
        return false;
    }

    return result->has_found_row_count;
}

uint64_t mylite_result_found_row_count(const mylite_result *result) {
    if (result == NULL) {
        return 0U;
    }

    return result->found_row_count;
}

size_t mylite_result_column_count(const mylite_result *result) {
    if (result == NULL) {
        return 0U;
    }

    return result->column_count;
}

const char *mylite_result_column_name(const mylite_result *result, size_t column_index) {
    if (result == NULL || column_index >= result->column_count) {
        return NULL;
    }

    return result->column_names[column_index];
}

const char *mylite_result_column_schema_name(const mylite_result *result, size_t column_index) {
    const struct mylite_result_column *column =
        mylite_result_column_metadata_at(result, column_index);

    return column == NULL ? NULL : column->schema_name;
}

const char *mylite_result_column_table_name(const mylite_result *result, size_t column_index) {
    const struct mylite_result_column *column =
        mylite_result_column_metadata_at(result, column_index);

    return column == NULL ? NULL : column->table_name;
}

const char *mylite_result_column_origin_schema_name(
    const mylite_result *result,
    size_t column_index
) {
    const struct mylite_result_column *column =
        mylite_result_column_metadata_at(result, column_index);

    return column == NULL ? NULL : column->origin_schema_name;
}

const char *mylite_result_column_origin_table_name(
    const mylite_result *result,
    size_t column_index
) {
    const struct mylite_result_column *column =
        mylite_result_column_metadata_at(result, column_index);

    return column == NULL ? NULL : column->origin_table_name;
}

const char *mylite_result_column_origin_name(const mylite_result *result, size_t column_index) {
    const struct mylite_result_column *column =
        mylite_result_column_metadata_at(result, column_index);

    return column == NULL ? NULL : column->origin_column_name;
}

enum mylite_result_column_type mylite_result_column_type(
    const mylite_result *result,
    size_t column_index
) {
    const struct mylite_result_column *column =
        mylite_result_column_metadata_at(result, column_index);

    return column == NULL ? MYLITE_RESULT_COLUMN_TYPE_UNKNOWN
                          : (enum mylite_result_column_type)column->logical_type;
}

uint32_t mylite_result_column_flags(const mylite_result *result, size_t column_index) {
    const struct mylite_result_column *column =
        mylite_result_column_metadata_at(result, column_index);

    return column == NULL ? 0U : column->flags;
}

uint32_t mylite_result_column_charset_id(const mylite_result *result, size_t column_index) {
    const struct mylite_result_column *column =
        mylite_result_column_metadata_at(result, column_index);

    return column == NULL ? 0U : column->charset_id;
}

uint32_t mylite_result_column_collation_id(const mylite_result *result, size_t column_index) {
    const struct mylite_result_column *column =
        mylite_result_column_metadata_at(result, column_index);

    return column == NULL ? 0U : column->collation_id;
}

uint64_t mylite_result_column_display_length(const mylite_result *result, size_t column_index) {
    const struct mylite_result_column *column =
        mylite_result_column_metadata_at(result, column_index);

    return column == NULL ? 0U : column->display_length;
}

uint16_t mylite_result_column_decimals(const mylite_result *result, size_t column_index) {
    const struct mylite_result_column *column =
        mylite_result_column_metadata_at(result, column_index);

    return column == NULL ? 0U : column->decimals;
}

int mylite_result_column_nullable(const mylite_result *result, size_t column_index) {
    const struct mylite_result_column *column =
        mylite_result_column_metadata_at(result, column_index);

    return column != NULL && column->nullable ? 1 : 0;
}

size_t mylite_result_row_count(const mylite_result *result) {
    if (result == NULL) {
        return 0U;
    }

    return result->row_count;
}

const char *mylite_result_value_text(
    const mylite_result *result,
    size_t row_index,
    size_t column_index
) {
    if (result == NULL || row_index >= result->row_count || column_index >= result->column_count) {
        return NULL;
    }

    return result->values[(row_index * result->column_count) + column_index];
}

int mylite_result_value_is_null(
    const mylite_result *result,
    size_t row_index,
    size_t column_index
) {
    if (result == NULL || row_index >= result->row_count || column_index >= result->column_count) {
        return 1;
    }

    return result->values[(row_index * result->column_count) + column_index] == NULL ? 1 : 0;
}

const void *mylite_result_value_bytes(
    const mylite_result *result,
    size_t row_index,
    size_t column_index
) {
    if (result == NULL || row_index >= result->row_count || column_index >= result->column_count) {
        return NULL;
    }

    return result->values[(row_index * result->column_count) + column_index];
}

size_t mylite_result_value_size(
    const mylite_result *result,
    size_t row_index,
    size_t column_index
) {
    if (result == NULL || row_index >= result->row_count || column_index >= result->column_count) {
        return 0U;
    }

    return result->value_sizes[(row_index * result->column_count) + column_index];
}

int64_t mylite_result_affected_rows(const mylite_result *result) {
    if (result == NULL) {
        return 0;
    }

    return result->affected_rows;
}

const char *mylite_result_info(const mylite_result *result) {
    if (result == NULL) {
        return NULL;
    }

    return result->info;
}

uint64_t mylite_result_insert_id(const mylite_result *result) {
    if (result == NULL) {
        return 0U;
    }

    return result->insert_id;
}

size_t mylite_result_warning_count(const mylite_result *result) {
    if (result == NULL) {
        return 0U;
    }

    return result->warning_count;
}

const struct mylite_result_column *mylite_result_column_metadata_at(
    const mylite_result *result,
    size_t column_index
) {
    if (result == NULL || column_index >= result->column_count ||
        mylite_result_metadata_column_count(&result->metadata) != result->column_count) {
        return NULL;
    }

    return mylite_result_metadata_column_at(&result->metadata, column_index);
}

static int reserve_columns(mylite_result *result, size_t required_capacity) {
    enum { initial_column_capacity = 4 };

    char **columns = NULL;
    size_t capacity = result->column_capacity;

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

    columns = (char **)realloc((void *)result->column_names, capacity * sizeof(*columns));
    if (columns == NULL) {
        return MYLITE_NOMEM;
    }

    for (size_t index = result->column_capacity; index < capacity; ++index) {
        columns[index] = NULL;
    }
    result->column_names = columns;
    result->column_capacity = capacity;

    return MYLITE_OK;
}

static int reserve_rows(mylite_result *result, size_t required_capacity) {
    enum { initial_row_capacity = 4 };

    char **values = NULL;
    size_t *value_sizes = NULL;
    size_t capacity = result->row_capacity;
    size_t old_value_capacity = 0U;

    if (required_capacity <= capacity) {
        return MYLITE_OK;
    }
    if (capacity == 0U) {
        capacity = initial_row_capacity;
    }
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (result->column_count != 0U && capacity > SIZE_MAX / result->column_count) {
        return MYLITE_NOMEM;
    }
    if ((capacity * result->column_count) > SIZE_MAX / sizeof(*values)) {
        return MYLITE_NOMEM;
    }

    old_value_capacity = result->row_capacity * result->column_count;
    values =
        (char **)realloc((void *)result->values, capacity * result->column_count * sizeof(*values));
    if (values == NULL) {
        return MYLITE_NOMEM;
    }
    value_sizes = (size_t *)realloc(
        (void *)result->value_sizes,
        capacity * result->column_count * sizeof(*value_sizes)
    );
    if (value_sizes == NULL) {
        result->values = values;
        return MYLITE_NOMEM;
    }

    for (size_t index = old_value_capacity; index < capacity * result->column_count; ++index) {
        values[index] = NULL;
        value_sizes[index] = 0U;
    }
    result->values = values;
    result->value_sizes = value_sizes;
    result->row_capacity = capacity;

    return MYLITE_OK;
}

static int duplicate_bytes(const void *bytes, size_t size, char **out_copy) {
    char *copy = NULL;

    if (out_copy == NULL || (bytes == NULL && size != 0U) || size == SIZE_MAX) {
        return MYLITE_MISUSE;
    }
    *out_copy = NULL;

    copy = malloc(size + 1U);
    if (copy == NULL) {
        return MYLITE_NOMEM;
    }
    if (size != 0U) {
        memcpy(copy, bytes, size);
    }
    copy[size] = '\0';
    *out_copy = copy;

    return MYLITE_OK;
}

static char *duplicate_text(const char *text) {
    char *copy = NULL;
    size_t length = 0U;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    if (length == SIZE_MAX) {
        return NULL;
    }

    copy = malloc(length + 1U);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1U);

    return copy;
}

static void free_values(char **values, size_t value_count) {
    if (values == NULL) {
        return;
    }

    for (size_t index = 0U; index < value_count; ++index) {
        free(values[index]);
    }
    free((void *)values);
}
