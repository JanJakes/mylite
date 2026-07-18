#ifndef MYLITE_SQL_MYLITE_SOURCE_SPAN_H
#define MYLITE_SQL_MYLITE_SOURCE_SPAN_H

#include <stdbool.h>
#include <stddef.h>

struct mylite_sql_source_span {
    const char *text;
    size_t length;
    size_t offset;
    size_t source_length;
};

static inline bool mylite_sql_source_span_is_valid(struct mylite_sql_source_span span) {
    if (span.text == NULL) {
        return span.length == 0U && span.offset == 0U && span.source_length == 0U;
    }

    return span.offset <= span.source_length && span.length <= span.source_length - span.offset;
}

#endif
