#ifndef MYLITE_SQL_MYLITE_VERSION_COMMENT_H
#define MYLITE_SQL_MYLITE_VERSION_COMMENT_H

#include <stdbool.h>
#include <stddef.h>

struct mylite_sql_version_comment_payload {
    const char *text;
    size_t length;
    bool active;
};

bool mylite_sql_version_comment_parse(
    const char *text,
    size_t length,
    struct mylite_sql_version_comment_payload *out_payload
);

#endif
