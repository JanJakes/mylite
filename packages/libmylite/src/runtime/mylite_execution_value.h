#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_VALUE_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_VALUE_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_db;

enum mylite_stmt_binding_type {
    MYLITE_STMT_BINDING_UNBOUND = 0,
    MYLITE_STMT_BINDING_NULL,
    MYLITE_STMT_BINDING_INT64,
    MYLITE_STMT_BINDING_UINT64,
    MYLITE_STMT_BINDING_DOUBLE,
    MYLITE_STMT_BINDING_TEXT,
    MYLITE_STMT_BINDING_BLOB,
};

struct mylite_stmt_binding {
    enum mylite_stmt_binding_type type;

    union {
        int64_t int64_value;
        uint64_t uint64_value;
        double double_value;
    } scalar;

    unsigned char *bytes;
    size_t size;
    size_t capacity;
};

struct planned_value {
    bool is_null;
    bool is_text;
    bool is_blob;
    bool is_real;
    bool is_external_parameter;
    int64_t integer;
    double real;
    char *text;
    size_t text_length;
    size_t external_parameter_index;
    const struct mylite_stmt_binding *external_binding;
};

void planned_value_deinit(struct planned_value *value);
int copy_planned_value(
    struct mylite_db *database,
    const struct planned_value *source,
    struct planned_value *out_value
);
int bind_planned_value_parameter(
    sqlite3_stmt *statement,
    int parameter_index,
    const struct planned_value *value
);
int bind_stmt_binding_parameter(
    sqlite3_stmt *statement,
    int parameter_index,
    const struct mylite_stmt_binding *binding
);
int validate_sqlite_parameter_count(sqlite3_stmt *statement, int bound_parameter_count);
int bind_int64_parameter(sqlite3_stmt *statement, int parameter_index, int64_t value);
int bind_text_parameter(sqlite3_stmt *statement, int parameter_index, const char *value);

#endif
