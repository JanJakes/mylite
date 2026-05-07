#ifndef MYLITE_RUNTIME_MYLITE_SHOW_CREATE_TABLE_CHECKS_H
#define MYLITE_RUNTIME_MYLITE_SHOW_CREATE_TABLE_CHECKS_H

#include "mylite_show_create_table_target.h"
#include "sqlite3.h"

#include <mylite/mylite.h>

#include <stdbool.h>

int mylite_show_create_table_append_checks(
    mylite_db *database,
    sqlite3_str *create_sql,
    const struct mylite_show_create_table_target *target,
    bool *first_line
);

#endif
