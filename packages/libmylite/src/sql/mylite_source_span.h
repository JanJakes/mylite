#ifndef MYLITE_SQL_MYLITE_SOURCE_SPAN_H
#define MYLITE_SQL_MYLITE_SOURCE_SPAN_H

#include <stddef.h>

struct mylite_sql_source_span {
    const char *text;
    size_t length;
    size_t offset;
    size_t line;
    size_t column;
};

#endif
