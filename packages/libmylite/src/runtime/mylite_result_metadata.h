#ifndef MYLITE_RUNTIME_MYLITE_RESULT_METADATA_H
#define MYLITE_RUNTIME_MYLITE_RESULT_METADATA_H

#include <mylite/mylite.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum mylite_result_logical_type {
    MYLITE_RESULT_LOGICAL_TYPE_UNKNOWN = MYLITE_RESULT_COLUMN_TYPE_UNKNOWN,
    MYLITE_RESULT_LOGICAL_TYPE_DECIMAL = MYLITE_RESULT_COLUMN_TYPE_DECIMAL,
    MYLITE_RESULT_LOGICAL_TYPE_TINY = MYLITE_RESULT_COLUMN_TYPE_TINY,
    MYLITE_RESULT_LOGICAL_TYPE_SHORT = MYLITE_RESULT_COLUMN_TYPE_SHORT,
    MYLITE_RESULT_LOGICAL_TYPE_LONG = MYLITE_RESULT_COLUMN_TYPE_LONG,
    MYLITE_RESULT_LOGICAL_TYPE_FLOAT = MYLITE_RESULT_COLUMN_TYPE_FLOAT,
    MYLITE_RESULT_LOGICAL_TYPE_DOUBLE = MYLITE_RESULT_COLUMN_TYPE_DOUBLE,
    MYLITE_RESULT_LOGICAL_TYPE_NULL = MYLITE_RESULT_COLUMN_TYPE_NULL,
    MYLITE_RESULT_LOGICAL_TYPE_TIMESTAMP = MYLITE_RESULT_COLUMN_TYPE_TIMESTAMP,
    MYLITE_RESULT_LOGICAL_TYPE_LONGLONG = MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
    MYLITE_RESULT_LOGICAL_TYPE_INT24 = MYLITE_RESULT_COLUMN_TYPE_INT24,
    MYLITE_RESULT_LOGICAL_TYPE_DATE = MYLITE_RESULT_COLUMN_TYPE_DATE,
    MYLITE_RESULT_LOGICAL_TYPE_TIME = MYLITE_RESULT_COLUMN_TYPE_TIME,
    MYLITE_RESULT_LOGICAL_TYPE_DATETIME = MYLITE_RESULT_COLUMN_TYPE_DATETIME,
    MYLITE_RESULT_LOGICAL_TYPE_YEAR = MYLITE_RESULT_COLUMN_TYPE_YEAR,
    MYLITE_RESULT_LOGICAL_TYPE_VARCHAR = MYLITE_RESULT_COLUMN_TYPE_VARCHAR,
    MYLITE_RESULT_LOGICAL_TYPE_BIT = MYLITE_RESULT_COLUMN_TYPE_BIT,
    MYLITE_RESULT_LOGICAL_TYPE_JSON = MYLITE_RESULT_COLUMN_TYPE_JSON,
    MYLITE_RESULT_LOGICAL_TYPE_NEWDECIMAL = MYLITE_RESULT_COLUMN_TYPE_NEWDECIMAL,
    MYLITE_RESULT_LOGICAL_TYPE_TINY_BLOB = MYLITE_RESULT_COLUMN_TYPE_TINY_BLOB,
    MYLITE_RESULT_LOGICAL_TYPE_MEDIUM_BLOB = MYLITE_RESULT_COLUMN_TYPE_MEDIUM_BLOB,
    MYLITE_RESULT_LOGICAL_TYPE_LONG_BLOB = MYLITE_RESULT_COLUMN_TYPE_LONG_BLOB,
    MYLITE_RESULT_LOGICAL_TYPE_BLOB = MYLITE_RESULT_COLUMN_TYPE_BLOB,
    MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING = MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
    MYLITE_RESULT_LOGICAL_TYPE_STRING = MYLITE_RESULT_COLUMN_TYPE_STRING,
    MYLITE_RESULT_LOGICAL_TYPE_GEOMETRY = MYLITE_RESULT_COLUMN_TYPE_GEOMETRY,
};

struct mylite_result_column_descriptor {
    const char *label;
    const char *schema_name;
    const char *table_name;
    const char *origin_schema_name;
    const char *origin_table_name;
    const char *origin_column_name;
    enum mylite_result_logical_type logical_type;
    uint32_t flags;
    uint32_t charset_id;
    uint32_t collation_id;
    uint64_t display_length;
    uint16_t decimals;
    bool nullable;
};

struct mylite_result_column {
    char *label;
    char *schema_name;
    char *table_name;
    char *origin_schema_name;
    char *origin_table_name;
    char *origin_column_name;
    enum mylite_result_logical_type logical_type;
    uint32_t flags;
    uint32_t charset_id;
    uint32_t collation_id;
    uint64_t display_length;
    uint16_t decimals;
    bool nullable;
};

struct mylite_result_metadata {
    struct mylite_result_column *columns;
    size_t column_count;
    size_t column_capacity;
};

void mylite_result_metadata_init(struct mylite_result_metadata *metadata);
void mylite_result_metadata_deinit(struct mylite_result_metadata *metadata);

int mylite_result_metadata_append(
    struct mylite_result_metadata *metadata,
    const struct mylite_result_column_descriptor *descriptor
);
void mylite_result_metadata_remove_last(struct mylite_result_metadata *metadata);

size_t mylite_result_metadata_column_count(const struct mylite_result_metadata *metadata);
const struct mylite_result_column *mylite_result_metadata_column_at(
    const struct mylite_result_metadata *metadata,
    size_t index
);

#endif
