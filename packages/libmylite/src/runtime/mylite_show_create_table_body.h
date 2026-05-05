#ifndef MYLITE_RUNTIME_MYLITE_SHOW_CREATE_TABLE_BODY_H
#define MYLITE_RUNTIME_MYLITE_SHOW_CREATE_TABLE_BODY_H

#include "sqlite3.h"

#include <stdbool.h>

void mylite_show_create_table_append_line_prefix(sqlite3_str *create_sql, bool *first_line);

#endif
