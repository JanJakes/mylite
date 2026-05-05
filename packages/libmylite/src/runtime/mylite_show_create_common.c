#include "mylite_show_create_common.h"

#include <stddef.h>

void mylite_show_create_append_identifier(sqlite3_str *create_sql, const char *identifier)
{
    sqlite3_str_appendchar(create_sql, 1, '`');
    for (const char *cursor = identifier == NULL ? "" : identifier; *cursor != '\0'; ++cursor) {
        if (*cursor == '`') {
            sqlite3_str_appendall(create_sql, "``");
        } else {
            sqlite3_str_appendchar(create_sql, 1, *cursor);
        }
    }
    sqlite3_str_appendchar(create_sql, 1, '`');
}

void mylite_show_create_append_string_literal(sqlite3_str *create_sql, const char *text)
{
    sqlite3_str_appendchar(create_sql, 1, '\'');
    for (const char *cursor = text == NULL ? "" : text; *cursor != '\0'; ++cursor) {
        if (*cursor == '\'' || *cursor == '\\') {
            sqlite3_str_appendchar(create_sql, 1, '\\');
        }
        sqlite3_str_appendchar(create_sql, 1, *cursor);
    }
    sqlite3_str_appendchar(create_sql, 1, '\'');
}

sqlite3_destructor_type mylite_show_sqlite_transient_destructor(void)
{
    return SQLITE_TRANSIENT; // NOLINT(performance-no-int-to-ptr)
}
