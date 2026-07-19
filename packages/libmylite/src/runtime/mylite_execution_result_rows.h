#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_RESULT_ROWS_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_RESULT_ROWS_H

#include <mylite/mylite.h>

#include <stddef.h>

struct mylite_catalog_column_descriptor;
struct mylite_db;
struct mylite_result_cell;
typedef struct sqlite3_stmt sqlite3_stmt;

struct mylite_execution_result_row_storage {
    struct mylite_result_cell *values;
    char *texts;
    size_t column_capacity;
    size_t text_capacity;
};

int mylite_execution_append_sqlite_result_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    mylite_result *result,
    const struct mylite_catalog_column_descriptor *const *columns,
    size_t descriptor_count,
    struct mylite_execution_result_row_storage *storage
);
int mylite_execution_read_sqlite_result_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_count,
    const struct mylite_catalog_column_descriptor *const *columns,
    size_t descriptor_count,
    struct mylite_execution_result_row_storage *storage
);
void mylite_execution_result_row_storage_deinit(struct mylite_execution_result_row_storage *storage
);
int mylite_execution_read_sqlite_result_cell(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *column,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
);
int mylite_execution_choose_sqlite_rowid_alias(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *unsupported_message,
    const char **out_alias
);

#endif
