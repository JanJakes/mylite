#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_VALUES_SUPPORT_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_VALUES_SUPPORT_H

#include <stddef.h>

struct mylite_db;
struct mylite_sql_ast_node;

int mylite_execution_values_resolve_column_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char *column_name,
    size_t column_name_size,
    char *display_name,
    size_t display_name_size,
    size_t *out_part_count
);

#endif
