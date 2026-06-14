#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_TEXT_INTERNAL_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_TEXT_INTERNAL_H

#include "mylite_ast.h"

#include <stddef.h>

struct mylite_db;

int mylite_execution_copy_source_span_text(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_text
);
int mylite_execution_copy_identifier_text(
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size,
    struct mylite_db *database
);
int mylite_execution_copy_quoted_identifier_text(
    const char *source,
    size_t source_size,
    char *destination,
    size_t destination_size
);
int mylite_execution_copy_unquoted_identifier_text(
    const char *source,
    size_t source_size,
    char *destination,
    size_t destination_size
);
int mylite_execution_duplicate_text(
    struct mylite_db *database,
    const char *source,
    char **out_text
);

static inline int copy_source_span_text(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_text
) {
    return mylite_execution_copy_source_span_text(database, span, out_text);
}

static inline int copy_identifier_text(
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size,
    struct mylite_db *database
) {
    return mylite_execution_copy_identifier_text(node, destination, destination_size, database);
}

static inline int copy_quoted_identifier_text(
    const char *source,
    size_t source_size,
    char *destination,
    size_t destination_size
) {
    return mylite_execution_copy_quoted_identifier_text(
        source,
        source_size,
        destination,
        destination_size
    );
}

static inline int copy_unquoted_identifier_text(
    const char *source,
    size_t source_size,
    char *destination,
    size_t destination_size
) {
    return mylite_execution_copy_unquoted_identifier_text(
        source,
        source_size,
        destination,
        destination_size
    );
}

static inline int duplicate_text(struct mylite_db *database, const char *source, char **out_text) {
    return mylite_execution_duplicate_text(database, source, out_text);
}

#endif
