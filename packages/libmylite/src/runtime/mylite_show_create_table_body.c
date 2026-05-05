#include "mylite_show_create_table_body.h"

#include "sqlite3.h"

void mylite_show_create_table_append_line_prefix(sqlite3_str *create_sql, bool *first_line)
{
    if (!*first_line) {
        sqlite3_str_appendall(create_sql, ",\n");
    }
    *first_line = false;
    sqlite3_str_appendall(create_sql, "  ");
}
