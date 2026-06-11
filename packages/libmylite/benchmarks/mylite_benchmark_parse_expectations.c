#include "mylite_benchmark_parse_expectations.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    expected_failure_initial_capacity = 32,
    expected_failure_capacity_growth_factor = 2,
    decimal_base = 10,
    max_query_index_field_length = 31,
};

struct expected_failure_fields {
    const char *query_index;
    size_t query_index_length;
    const char *status_name;
    size_t status_name_length;
    const char *token_kind_name;
    size_t token_kind_name_length;
    const char *reason;
    size_t reason_length;
};

static int read_file(const char *path, char **out_data, size_t *out_size);
static int parse_expected_failure_line(
    struct mylite_benchmark_expected_parse_failure_list *expectations,
    const char *source_name,
    size_t line_number,
    const char *line,
    size_t length
);
static int split_expected_failure_fields(
    const char *line,
    size_t length,
    struct expected_failure_fields *out_fields
);
static int parse_query_index_field(
    const char *source_name,
    size_t line_number,
    const char *text,
    size_t length,
    size_t *out_query_index
);
static bool parse_failure_status_name_is_valid(const char *text, size_t length);
static bool token_kind_name_is_valid(const char *text, size_t length);
static int append_expected_failure(
    struct mylite_benchmark_expected_parse_failure_list *expectations,
    const struct expected_failure_fields *fields,
    size_t query_index,
    const char *source_name,
    size_t line_number
);
static int copy_field(const char *text, size_t length, char **out_copy);
static bool string_equals_field(const char *string, const char *text, size_t length);

int mylite_benchmark_load_expected_parse_failures(
    const char *path,
    struct mylite_benchmark_expected_parse_failure_list *out_expectations
) {
    char *data = NULL;
    size_t data_size = 0U;
    int rc = read_file(path, &data, &data_size);

    if (rc != 0) {
        return rc;
    }
    rc = mylite_benchmark_parse_expected_parse_failures(data, data_size, path, out_expectations);
    free(data);
    if (rc != 0) {
        mylite_benchmark_expected_parse_failure_list_deinit(out_expectations);
    }
    return rc;
}

int mylite_benchmark_parse_expected_parse_failures(
    const char *data,
    size_t length,
    const char *source_name,
    struct mylite_benchmark_expected_parse_failure_list *out_expectations
) {
    size_t cursor = 0U;
    size_t line_number = 1U;

    if (data == NULL || source_name == NULL || out_expectations == NULL) {
        return 1;
    }
    while (cursor < length) {
        const char *line = &data[cursor];
        size_t line_length = 0U;
        int rc = 0;

        while (cursor + line_length < length && line[line_length] != '\r' &&
               line[line_length] != '\n') {
            ++line_length;
        }
        rc = parse_expected_failure_line(
            out_expectations,
            source_name,
            line_number,
            line,
            line_length
        );
        if (rc != 0) {
            mylite_benchmark_expected_parse_failure_list_deinit(out_expectations);
            return rc;
        }
        cursor += line_length;
        if (cursor < length && data[cursor] == '\r') {
            ++cursor;
        }
        if (cursor < length && data[cursor] == '\n') {
            ++cursor;
        }
        ++line_number;
    }
    return 0;
}

void mylite_benchmark_expected_parse_failure_list_deinit(
    struct mylite_benchmark_expected_parse_failure_list *expectations
) {
    if (expectations == NULL) {
        return;
    }
    for (size_t index = 0U; index < expectations->count; ++index) {
        free(expectations->items[index].status_name);
        free(expectations->items[index].token_kind_name);
        free(expectations->items[index].reason);
    }
    free(expectations->items);
    expectations->items = NULL;
    expectations->count = 0U;
    expectations->capacity = 0U;
}

const struct mylite_benchmark_expected_parse_failure *mylite_benchmark_expected_parse_failure_find(
    const struct mylite_benchmark_expected_parse_failure_list *expectations,
    size_t query_index
) {
    if (expectations == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < expectations->count; ++index) {
        if (expectations->items[index].query_index == query_index) {
            return &expectations->items[index];
        }
    }
    return NULL;
}

bool mylite_benchmark_expected_parse_failure_matches(
    const struct mylite_benchmark_expected_parse_failure *expectation,
    enum mylite_sql_parse_status status,
    enum mylite_sql_token_kind token_kind
) {
    return expectation != NULL &&
           strcmp(expectation->status_name, mylite_sql_parse_status_name(status)) == 0 &&
           strcmp(expectation->token_kind_name, mylite_sql_token_kind_name(token_kind)) == 0;
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

static int parse_expected_failure_line(
    struct mylite_benchmark_expected_parse_failure_list *expectations,
    const char *source_name,
    size_t line_number,
    const char *line,
    size_t length
) {
    struct expected_failure_fields fields = {0};
    size_t query_index = 0U;
    int rc = 0;

    if (length == 0U || line[0] == '#') {
        return 0;
    }
    rc = split_expected_failure_fields(line, length, &fields);
    if (rc != 0) {
        fprintf(stderr, "%s:%zu: expected four tab-separated fields\n", source_name, line_number);
        return 1;
    }
    rc = parse_query_index_field(
        source_name,
        line_number,
        fields.query_index,
        fields.query_index_length,
        &query_index
    );
    if (rc != 0) {
        return rc;
    }
    if (!parse_failure_status_name_is_valid(fields.status_name, fields.status_name_length)) {
        fprintf(stderr, "%s:%zu: invalid parse status\n", source_name, line_number);
        return 1;
    }
    if (!token_kind_name_is_valid(fields.token_kind_name, fields.token_kind_name_length)) {
        fprintf(stderr, "%s:%zu: invalid token kind\n", source_name, line_number);
        return 1;
    }
    if (fields.reason_length == 0U) {
        fprintf(stderr, "%s:%zu: expected nonempty reason\n", source_name, line_number);
        return 1;
    }
    return append_expected_failure(expectations, &fields, query_index, source_name, line_number);
}

static int split_expected_failure_fields(
    const char *line,
    size_t length,
    struct expected_failure_fields *out_fields
) {
    size_t first_tab = SIZE_MAX;
    size_t second_tab = SIZE_MAX;
    size_t third_tab = SIZE_MAX;

    for (size_t index = 0U; index < length; ++index) {
        if (line[index] != '\t') {
            continue;
        }
        if (first_tab == SIZE_MAX) {
            first_tab = index;
        } else if (second_tab == SIZE_MAX) {
            second_tab = index;
        } else {
            third_tab = index;
            break;
        }
    }
    if (first_tab == SIZE_MAX || second_tab == SIZE_MAX || third_tab == SIZE_MAX) {
        return 1;
    }
    out_fields->query_index = line;
    out_fields->query_index_length = first_tab;
    out_fields->status_name = &line[first_tab + 1U];
    out_fields->status_name_length = second_tab - first_tab - 1U;
    out_fields->token_kind_name = &line[second_tab + 1U];
    out_fields->token_kind_name_length = third_tab - second_tab - 1U;
    out_fields->reason = &line[third_tab + 1U];
    out_fields->reason_length = length - third_tab - 1U;
    return 0;
}

static int parse_query_index_field(
    const char *source_name,
    size_t line_number,
    const char *text,
    size_t length,
    size_t *out_query_index
) {
    char buffer[max_query_index_field_length + 1U];
    char *end = NULL;
    unsigned long long value = 0ULL;

    if (length == 0U || length > max_query_index_field_length) {
        fprintf(stderr, "%s:%zu: invalid query index\n", source_name, line_number);
        return 1;
    }
    memcpy(buffer, text, length);
    buffer[length] = '\0';
    errno = 0;
    value = strtoull(buffer, &end, decimal_base);
    if (errno != 0 || end == buffer || *end != '\0' || value == 0ULL ||
        value > (unsigned long long)SIZE_MAX) {
        fprintf(stderr, "%s:%zu: invalid query index\n", source_name, line_number);
        return 1;
    }
    *out_query_index = (size_t)value;
    return 0;
}

static bool parse_failure_status_name_is_valid(const char *text, size_t length) {
    for (int status = MYLITE_SQL_PARSE_MISUSE; status <= MYLITE_SQL_PARSE_STACK_OVERFLOW;
         ++status) {
        if (string_equals_field(
                mylite_sql_parse_status_name((enum mylite_sql_parse_status)status),
                text,
                length
            )) {
            return true;
        }
    }
    return false;
}

static bool token_kind_name_is_valid(const char *text, size_t length) {
    for (int kind = MYLITE_SQL_TOKEN_EOF; kind <= MYLITE_SQL_TOKEN_TEMPORAL_LITERAL_INTRODUCER;
         ++kind) {
        if (string_equals_field(
                mylite_sql_token_kind_name((enum mylite_sql_token_kind)kind),
                text,
                length
            )) {
            return true;
        }
    }
    return false;
}

static int append_expected_failure(
    struct mylite_benchmark_expected_parse_failure_list *expectations,
    const struct expected_failure_fields *fields,
    size_t query_index,
    const char *source_name,
    size_t line_number
) {
    struct mylite_benchmark_expected_parse_failure *items = NULL;
    struct mylite_benchmark_expected_parse_failure expectation = {0};
    size_t new_capacity = 0U;
    int rc = 0;

    if (mylite_benchmark_expected_parse_failure_find(expectations, query_index) != NULL) {
        fprintf(
            stderr,
            "%s:%zu: duplicate query index %zu\n",
            source_name,
            line_number,
            query_index
        );
        return 1;
    }
    rc = copy_field(fields->status_name, fields->status_name_length, &expectation.status_name);
    if (rc == 0) {
        rc = copy_field(
            fields->token_kind_name,
            fields->token_kind_name_length,
            &expectation.token_kind_name
        );
    }
    if (rc == 0) {
        rc = copy_field(fields->reason, fields->reason_length, &expectation.reason);
    }
    if (rc != 0) {
        free(expectation.status_name);
        free(expectation.token_kind_name);
        free(expectation.reason);
        return 1;
    }
    if (expectations->count == expectations->capacity) {
        new_capacity = expectations->capacity == 0U
                           ? expected_failure_initial_capacity
                           : expectations->capacity * expected_failure_capacity_growth_factor;
        if (new_capacity < expectations->capacity ||
            new_capacity > SIZE_MAX / sizeof(*expectations->items)) {
            fprintf(stderr, "expected failure list capacity overflow\n");
            free(expectation.status_name);
            free(expectation.token_kind_name);
            free(expectation.reason);
            return 1;
        }
        items = (struct mylite_benchmark_expected_parse_failure *)
            realloc(expectations->items, new_capacity * sizeof(*items));
        if (items == NULL) {
            fprintf(stderr, "out of memory while growing expected failure list\n");
            free(expectation.status_name);
            free(expectation.token_kind_name);
            free(expectation.reason);
            return 1;
        }
        expectations->items = items;
        expectations->capacity = new_capacity;
    }
    expectation.query_index = query_index;
    expectations->items[expectations->count] = expectation;
    ++expectations->count;
    return 0;
}

static int copy_field(const char *text, size_t length, char **out_copy) {
    char *copy = (char *)malloc(length + 1U);

    if (copy == NULL) {
        fprintf(stderr, "out of memory while copying expected failure field\n");
        return 1;
    }
    memcpy(copy, text, length);
    copy[length] = '\0';
    *out_copy = copy;
    return 0;
}

static bool string_equals_field(const char *string, const char *text, size_t length) {
    return strlen(string) == length && memcmp(string, text, length) == 0;
}
