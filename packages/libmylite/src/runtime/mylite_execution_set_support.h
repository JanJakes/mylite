#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SET_SUPPORT_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SET_SUPPORT_H

#include <stddef.h>

struct mylite_db;
struct mylite_sql_ast_node;
struct session_scalar_cell;

int mylite_execution_set_copy_table_option_name_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size,
    const char *identifier_kind,
    const char *nul_message
);
int mylite_execution_set_decode_table_option_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char **out_name,
    size_t *out_name_length,
    const char *identifier_kind,
    const char *nul_message
);
int mylite_execution_set_append_utf8_alias_warning(struct mylite_db *database);
int mylite_execution_set_append_utf8mb3_deprecation_warning(struct mylite_db *database);

#endif
