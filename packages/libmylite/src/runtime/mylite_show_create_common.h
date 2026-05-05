#ifndef MYLITE_RUNTIME_MYLITE_SHOW_CREATE_COMMON_H
#define MYLITE_RUNTIME_MYLITE_SHOW_CREATE_COMMON_H

#include "sqlite3.h"

void mylite_show_create_append_identifier(sqlite3_str *create_sql, const char *identifier);
void mylite_show_create_append_string_literal(sqlite3_str *create_sql, const char *text);
sqlite3_destructor_type mylite_show_sqlite_transient_destructor(void);

#endif
