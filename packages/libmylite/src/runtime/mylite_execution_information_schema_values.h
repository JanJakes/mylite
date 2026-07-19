#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_VALUES_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_VALUES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_db;
struct mylite_execution_catalog_column_definition;
struct mylite_execution_catalog_table_definition;

int mylite_execution_information_schema_compare_text(
    const struct mylite_execution_catalog_table_definition *definition,
    size_t column_index,
    const char *left,
    const char *right
);
int mylite_execution_information_schema_compare_column_text(
    const struct mylite_execution_catalog_column_definition *column,
    const char *left,
    const char *right
);
int mylite_execution_information_schema_compare_ascii_case_insensitive_text(
    const char *left,
    const char *right
);
bool mylite_execution_information_schema_column_uses_case_insensitive_collation(
    const struct mylite_execution_catalog_column_definition *column
);
bool mylite_execution_information_schema_column_is_numeric(
    const struct mylite_execution_catalog_table_definition *definition,
    size_t column_index
);
int mylite_execution_information_schema_text_to_i64(const char *text, int64_t *out_value);
int mylite_execution_information_schema_format_i64(
    struct mylite_db *database,
    int64_t value,
    char *buffer,
    size_t buffer_size
);

static inline int information_schema_compare_text(
    const struct mylite_execution_catalog_table_definition *definition,
    size_t column_index,
    const char *left,
    const char *right
) {
    return mylite_execution_information_schema_compare_text(definition, column_index, left, right);
}

static inline int information_schema_compare_column_text(
    const struct mylite_execution_catalog_column_definition *column,
    const char *left,
    const char *right
) {
    return mylite_execution_information_schema_compare_column_text(column, left, right);
}

static inline int compare_ascii_case_insensitive_text(const char *left, const char *right) {
    return mylite_execution_information_schema_compare_ascii_case_insensitive_text(left, right);
}

static inline bool information_schema_column_uses_case_insensitive_collation(
    const struct mylite_execution_catalog_column_definition *column
) {
    return mylite_execution_information_schema_column_uses_case_insensitive_collation(column);
}

static inline bool information_schema_column_is_numeric(
    const struct mylite_execution_catalog_table_definition *definition,
    size_t column_index
) {
    return mylite_execution_information_schema_column_is_numeric(definition, column_index);
}

static inline int information_schema_text_to_i64(const char *text, int64_t *out_value) {
    return mylite_execution_information_schema_text_to_i64(text, out_value);
}

static inline int information_schema_format_i64(
    struct mylite_db *database,
    int64_t value,
    char *buffer,
    size_t buffer_size
) {
    return mylite_execution_information_schema_format_i64(database, value, buffer, buffer_size);
}

#endif
