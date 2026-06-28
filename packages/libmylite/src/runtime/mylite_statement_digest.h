#ifndef MYLITE_RUNTIME_MYLITE_STATEMENT_DIGEST_H
#define MYLITE_RUNTIME_MYLITE_STATEMENT_DIGEST_H

#include <mylite/mylite.h>

#include <stddef.h>

int mylite_statement_digest_text(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    char **out_text,
    size_t *out_text_size
);

#endif
