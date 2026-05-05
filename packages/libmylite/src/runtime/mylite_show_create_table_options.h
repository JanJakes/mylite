#ifndef MYLITE_RUNTIME_MYLITE_SHOW_CREATE_TABLE_OPTIONS_H
#define MYLITE_RUNTIME_MYLITE_SHOW_CREATE_TABLE_OPTIONS_H

#include "sqlite3.h"

struct mylite_show_create_table_info;

void mylite_show_create_table_append_options(sqlite3_str *create_sql,
                                             const struct mylite_show_create_table_info *info);

#endif
