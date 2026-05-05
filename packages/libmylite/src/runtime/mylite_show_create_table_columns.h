#ifndef MYLITE_RUNTIME_MYLITE_SHOW_CREATE_TABLE_COLUMNS_H
#define MYLITE_RUNTIME_MYLITE_SHOW_CREATE_TABLE_COLUMNS_H

#include "sqlite3.h"

#include <mylite/mylite.h>

#include <stdbool.h>

struct mylite_show_create_table_info;
struct mylite_show_create_table_target;

int mylite_show_create_table_append_columns(mylite_db *database, sqlite3_str *create_sql,
                                            const struct mylite_show_create_table_target *target,
                                            const struct mylite_show_create_table_info *info,
                                            bool *first_line);

#endif
