#include "mylite_benchmark_csv.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    owned_query_initial_capacity = 128,
    field_builder_initial_capacity = 256,
    capacity_growth_factor = 2,
};

struct field_builder {
    char *data;
    size_t length;
    size_t capacity;
};

static int read_file(const char *path, char **out_data, size_t *out_size);
static int parse_quoted_csv_field(
    const char *data,
    size_t length,
    size_t *cursor,
    struct field_builder *field
);
static int parse_unquoted_csv_field(
    const char *data,
    size_t length,
    size_t *cursor,
    struct field_builder *field
);
static int owned_query_list_append(
    struct mylite_benchmark_owned_query_list *queries,
    const char *sql,
    size_t length
);
static int field_builder_append(struct field_builder *field, char byte);
static void field_builder_clear(struct field_builder *field);
static void field_builder_deinit(struct field_builder *field);

int mylite_benchmark_load_csv_queries(
    const char *path,
    struct mylite_benchmark_owned_query_list *out_queries
) {
    char *data = NULL;
    size_t data_size = 0U;
    int rc = read_file(path, &data, &data_size);

    if (rc != 0) {
        return rc;
    }
    rc = mylite_benchmark_parse_single_column_csv(data, data_size, out_queries);
    free(data);
    if (rc != 0) {
        mylite_benchmark_owned_query_list_deinit(out_queries);
    }
    return rc;
}

int mylite_benchmark_parse_single_column_csv(
    const char *data,
    size_t length,
    struct mylite_benchmark_owned_query_list *out_queries
) {
    struct field_builder field = {0};
    size_t cursor = 0U;
    int rc = 0;

    while (cursor < length) {
        field_builder_clear(&field);
        if (data[cursor] == '\r' || data[cursor] == '\n') {
            ++cursor;
            continue;
        }

        if (data[cursor] == '"') {
            rc = parse_quoted_csv_field(data, length, &cursor, &field);
        } else {
            rc = parse_unquoted_csv_field(data, length, &cursor, &field);
        }
        if (rc != 0) {
            field_builder_deinit(&field);
            return rc;
        }
        while (cursor < length && data[cursor] != '\n') {
            ++cursor;
        }
        if (cursor < length && data[cursor] == '\n') {
            ++cursor;
        }
        if (field.length > 0U) {
            rc = owned_query_list_append(out_queries, field.data, field.length);
            if (rc != 0) {
                field_builder_deinit(&field);
                return rc;
            }
        }
    }

    field_builder_deinit(&field);
    return 0;
}

void mylite_benchmark_owned_query_list_deinit(struct mylite_benchmark_owned_query_list *queries) {
    for (size_t index = 0U; index < queries->count; ++index) {
        free(queries->items[index].sql);
    }
    free(queries->items);
    queries->items = NULL;
    queries->count = 0U;
    queries->capacity = 0U;
}

static int read_file(const char *path, char **out_data, size_t *out_size) {
    FILE *file = fopen(path, "rb");
    long file_size_long = 0L;
    size_t file_size = 0U;
    char *data = NULL;

    *out_data = NULL;
    *out_size = 0U;
    if (file == NULL) {
        fprintf(stderr, "%s: failed to open: %s\n", path, strerror(errno));
        return 1;
    }
    if (fseek(file, 0L, SEEK_END) != 0) {
        fprintf(stderr, "%s: failed to seek\n", path);
        fclose(file);
        return 1;
    }
    file_size_long = ftell(file);
    if (file_size_long < 0L) {
        fprintf(stderr, "%s: failed to read file size\n", path);
        fclose(file);
        return 1;
    }
    if (fseek(file, 0L, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to rewind\n", path);
        fclose(file);
        return 1;
    }
    file_size = (size_t)file_size_long;
    if (file_size == SIZE_MAX) {
        fprintf(stderr, "%s: file is too large\n", path);
        fclose(file);
        return 1;
    }
    data = (char *)malloc(file_size + 1U);
    if (data == NULL) {
        fprintf(stderr, "%s: out of memory\n", path);
        fclose(file);
        return 1;
    }
    if (file_size > 0U && fread(data, 1U, file_size, file) != file_size) {
        fprintf(stderr, "%s: failed to read file\n", path);
        free(data);
        fclose(file);
        return 1;
    }
    data[file_size] = '\0';
    if (fclose(file) != 0) {
        fprintf(stderr, "%s: failed to close file\n", path);
        free(data);
        return 1;
    }
    *out_data = data;
    *out_size = file_size;
    return 0;
}

static int parse_quoted_csv_field(
    const char *data,
    size_t length,
    size_t *cursor,
    struct field_builder *field
) {
    ++*cursor;
    while (*cursor < length) {
        char byte = data[*cursor];

        ++*cursor;
        if (byte == '\\' && *cursor < length && data[*cursor] == '"') {
            if (field_builder_append(field, byte) != 0 ||
                field_builder_append(field, data[*cursor]) != 0) {
                return 1;
            }
            ++*cursor;
            continue;
        }
        if (byte == '"') {
            if (*cursor < length && data[*cursor] == '"') {
                ++*cursor;
            } else {
                return 0;
            }
        }
        if (field_builder_append(field, byte) != 0) {
            return 1;
        }
    }

    fprintf(stderr, "unterminated quoted CSV field\n");
    return 1;
}

static int parse_unquoted_csv_field(
    const char *data,
    size_t length,
    size_t *cursor,
    struct field_builder *field
) {
    while (*cursor < length && data[*cursor] != '\r' && data[*cursor] != '\n') {
        if (field_builder_append(field, data[*cursor]) != 0) {
            return 1;
        }
        ++*cursor;
    }
    return 0;
}

static int owned_query_list_append(
    struct mylite_benchmark_owned_query_list *queries,
    const char *sql,
    size_t length
) {
    char *copy = NULL;
    struct mylite_benchmark_owned_query *items = NULL;
    size_t new_capacity = 0U;

    if (queries->count == queries->capacity) {
        new_capacity = queries->capacity == 0U ? owned_query_initial_capacity
                                               : queries->capacity * capacity_growth_factor;
        if (new_capacity < queries->capacity || new_capacity > SIZE_MAX / sizeof(*queries->items)) {
            fprintf(stderr, "query list capacity overflow\n");
            return 1;
        }
        items = (struct mylite_benchmark_owned_query *)
            realloc(queries->items, new_capacity * sizeof(*items));
        if (items == NULL) {
            fprintf(stderr, "out of memory while growing query list\n");
            return 1;
        }
        queries->items = items;
        queries->capacity = new_capacity;
    }
    copy = (char *)malloc(length + 1U);
    if (copy == NULL) {
        fprintf(stderr, "out of memory while copying query\n");
        return 1;
    }
    if (length > 0U) {
        memcpy(copy, sql, length);
    }
    copy[length] = '\0';
    queries->items[queries->count] = (struct mylite_benchmark_owned_query){
        .sql = copy,
        .length = length,
    };
    ++queries->count;
    return 0;
}

static int field_builder_append(struct field_builder *field, char byte) {
    char *data = NULL;
    size_t new_capacity = 0U;

    if (field->length == field->capacity) {
        new_capacity = field->capacity == 0U ? field_builder_initial_capacity
                                             : field->capacity * capacity_growth_factor;
        if (new_capacity < field->capacity) {
            fprintf(stderr, "CSV field capacity overflow\n");
            return 1;
        }
        data = (char *)realloc(field->data, new_capacity);
        if (data == NULL) {
            fprintf(stderr, "out of memory while growing CSV field\n");
            return 1;
        }
        field->data = data;
        field->capacity = new_capacity;
    }
    field->data[field->length] = byte;
    ++field->length;
    return 0;
}

static void field_builder_clear(struct field_builder *field) {
    field->length = 0U;
}

static void field_builder_deinit(struct field_builder *field) {
    free(field->data);
    field->data = NULL;
    field->length = 0U;
    field->capacity = 0U;
}
