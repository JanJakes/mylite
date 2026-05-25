#ifndef MYLITE_RUNTIME_MYLITE_TEMPORAL_CONSTRUCTOR_H
#define MYLITE_RUNTIME_MYLITE_TEMPORAL_CONSTRUCTOR_H

#include <mylite/mylite.h>

#include "sqlite3.h"

#include <stdbool.h>
#include <stdint.h>

int mylite_from_days_value(
    struct mylite_db *database,
    int64_t day_number,
    bool is_null,
    char **out_text,
    bool *out_is_null
);
int mylite_makedate_value(
    struct mylite_db *database,
    int64_t year,
    bool year_is_null,
    int64_t day_of_year,
    bool day_of_year_is_null,
    char **out_text,
    bool *out_is_null
);

struct mylite_maketime_arguments {
    int64_t hour;
    bool hour_is_null;
    int64_t minute;
    bool minute_is_null;
    int64_t second;
    bool second_is_null;
};

int mylite_maketime_value(
    struct mylite_db *database,
    const struct mylite_maketime_arguments *arguments,
    char **out_text,
    bool *out_is_null
);
int mylite_sqlite_register_temporal_constructor_functions(sqlite3 *sqlite);

#endif
