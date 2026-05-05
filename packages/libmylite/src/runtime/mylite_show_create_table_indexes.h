#ifndef MYLITE_RUNTIME_MYLITE_SHOW_CREATE_TABLE_INDEXES_H
#define MYLITE_RUNTIME_MYLITE_SHOW_CREATE_TABLE_INDEXES_H

#include "sqlite3.h"

#include <mylite/mylite.h>

#include <stdbool.h>

struct mylite_show_create_table_target;

int mylite_show_create_table_append_indexes(mylite_db *database, sqlite3_str *create_sql,
                                            const struct mylite_show_create_table_target *target,
                                            bool *first_line);

#endif
