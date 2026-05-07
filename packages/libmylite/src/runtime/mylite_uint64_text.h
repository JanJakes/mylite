#ifndef MYLITE_RUNTIME_MYLITE_UINT64_TEXT_H
#define MYLITE_RUNTIME_MYLITE_UINT64_TEXT_H

#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int mylite_sqlite_bind_uint64(sqlite3_stmt *stmt, int index, uint64_t value);
bool mylite_sqlite_column_uint64(sqlite3_stmt *stmt, int column, uint64_t *out_value);
char *mylite_copy_uint64_text(uint64_t value);
int mylite_format_uint64(uint64_t value, char *buffer, size_t buffer_size, size_t *out_length);
bool mylite_parse_uint64_text(const char *text, size_t length, uint64_t *out_value);

#endif
