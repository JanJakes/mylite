#ifndef MYLITE_RUNTIME_MYLITE_PERIOD_FUNCTIONS_H
#define MYLITE_RUNTIME_MYLITE_PERIOD_FUNCTIONS_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>

int mylite_period_add_value(
    struct mylite_db *database,
    int64_t period,
    bool period_is_null,
    int64_t months,
    bool months_is_null,
    int64_t *out_value,
    bool *out_is_null
);
int mylite_period_diff_value(
    struct mylite_db *database,
    int64_t period1,
    bool period1_is_null,
    int64_t period2,
    bool period2_is_null,
    int64_t *out_value,
    bool *out_is_null
);
int mylite_sqlite_register_period_functions(sqlite3 *sqlite);

#endif
