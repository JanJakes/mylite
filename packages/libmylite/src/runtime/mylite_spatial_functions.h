#ifndef MYLITE_RUNTIME_MYLITE_SPATIAL_FUNCTIONS_H
#define MYLITE_RUNTIME_MYLITE_SPATIAL_FUNCTIONS_H

#include "mylite_spatial.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>

struct mylite_db;

struct mylite_spatial_sql_work_context {
    sqlite3 *sqlite;
    uint64_t deadline_ns;
    bool has_deadline;
};

typedef void (*mylite_spatial_sql_work_test_hook)(void *context);

int mylite_sqlite_register_spatial_functions(sqlite3 *sqlite);
void mylite_spatial_sql_work_control_init(
    struct mylite_spatial_sql_work_context *work,
    const struct mylite_db *database,
    sqlite3 *sqlite,
    struct mylite_spatial_work_control *control
);
void mylite_spatial_set_sql_work_test_hook(mylite_spatial_sql_work_test_hook hook, void *context);

#endif
